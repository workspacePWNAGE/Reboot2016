// AvatarRenderer service - RCCService-style thumbnail renderer over plain HTTP.
//
//   HTTP threads (cpp-httplib)  --->  JobQueue  --->  main thread (OpenGL) renders
//
// OpenGL contexts are bound to one thread, so every render happens on the main
// thread, one job at a time, in FIFO order. HTTP handler threads only validate
// the request, put a job in the queue and wait for the result.
//
// NOTE: no raylib in this file (see renderer.hpp). Needs httplib.h from
// https://github.com/yhirose/cpp-httplib next to it.

#define CPPHTTPLIB_THREAD_POOL_COUNT 80 // sync /render calls park a thread each while they wait
#include "httplib.h"

#include "renderer.hpp"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <condition_variable>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <mutex>
#include <random>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;

// ---------------------------------------------------------------------------
// Config
// ---------------------------------------------------------------------------

struct Config
{
    std::string contentDir = "content";
    std::string host = "0.0.0.0";
    int port = 8080;          // one of the ports Cloudflare proxies over plain HTTP
    std::string apiKey;       // empty = no auth
    size_t maxQueue = 64;     // jobs waiting (not counting the one rendering)
    int maxSize = 1024;       // largest accepted ?size=
    int syncWaitSeconds = 90; // Cloudflare drops proxied requests at 100s
    int resultTtlSeconds = 300;
};

static void usage(const char *argv0)
{
    std::printf(
        "usage: %s [--content DIR] [--port N] [--host ADDR] [--key SECRET]\n"
        "          [--max-queue N] [--max-size PX]\n"
        "  --content    folder containing fonts/ and textures/   (default: content)\n"
        "  --port       listen port                               (default: 8080)\n"
        "  --key        require this API key (X-Api-Key header or Bearer token);\n"
        "               can also be set with the RENDER_API_KEY environment variable\n",
        argv0);
}

static bool parseArgs(int argc, char **argv, Config &c)
{
    if (const char *k = std::getenv("RENDER_API_KEY"))
        c.apiKey = k;

    for (int i = 1; i < argc; i++)
    {
        std::string a = argv[i];
        if (a == "--help" || a == "-h")
            return false;
        if (i + 1 >= argc)
        {
            std::printf("missing value for %s\n", a.c_str());
            return false;
        }
        const char *v = argv[++i];

        if (a == "--content")
            c.contentDir = v;
        else if (a == "--host")
            c.host = v;
        else if (a == "--port")
            c.port = std::atoi(v);
        else if (a == "--key")
            c.apiKey = v;
        else if (a == "--max-queue")
            c.maxQueue = (size_t)std::max(1, std::atoi(v));
        else if (a == "--max-size")
            c.maxSize = std::max(16, std::atoi(v));
        else
        {
            std::printf("unknown option %s\n", a.c_str());
            return false;
        }
    }
    return true;
}

// ---------------------------------------------------------------------------
// Jobs + queue
// ---------------------------------------------------------------------------

enum class JobState
{
    Queued,
    Running,
    Done,
    Failed
};

static const char *stateName(JobState s)
{
    switch (s)
    {
    case JobState::Queued:
        return "queued";
    case JobState::Running:
        return "running";
    case JobState::Done:
        return "done";
    default:
        return "failed";
    }
}

struct Job
{
    std::string id;
    RenderRequest req;

    // Guards everything below. Lock order rule: never call a JobQueue method
    // while holding a job's mutex (the queue locks job mutexes itself).
    std::mutex m;
    std::condition_variable cv;
    JobState state = JobState::Queued;
    std::string image; // base64 PNG once Done
    std::string error; // once Failed
    Clock::time_point finishedAt;
};

class JobQueue
{
public:
    JobQueue(size_t maxPending, int ttlSeconds) : maxPending_(maxPending), ttl_(ttlSeconds)
    {
        std::random_device rd;
        std::seed_seq seq{rd(), rd(), static_cast<unsigned>(Clock::now().time_since_epoch().count())};
        rng_.seed(seq);
    }

    // nullptr when the queue is full or the server is shutting down.
    std::shared_ptr<Job> submit(const RenderRequest &req)
    {
        std::lock_guard<std::mutex> lk(m_);
        if (stopping_ || pending_.size() >= maxPending_)
            return nullptr;

        prune();

        auto job = std::make_shared<Job>();
        job->id = makeId();
        job->req = req;
        jobs_[job->id] = job;
        pending_.push_back(job);
        cv_.notify_one();
        return job;
    }

    std::shared_ptr<Job> find(const std::string &id)
    {
        std::lock_guard<std::mutex> lk(m_);
        auto it = jobs_.find(id);
        return it == jobs_.end() ? nullptr : it->second;
    }

    void forget(const std::string &id)
    {
        std::lock_guard<std::mutex> lk(m_);
        jobs_.erase(id);
    }

    // 1-based place in line, 0 if not waiting.
    int position(const std::shared_ptr<Job> &job)
    {
        std::lock_guard<std::mutex> lk(m_);
        for (size_t i = 0; i < pending_.size(); i++)
            if (pending_[i] == job)
                return (int)i + 1;
        return 0;
    }

    size_t pendingCount()
    {
        std::lock_guard<std::mutex> lk(m_);
        return pending_.size();
    }

    // Render thread: wait up to waitMs for the next job. nullptr on timeout.
    std::shared_ptr<Job> next(int waitMs)
    {
        std::unique_lock<std::mutex> lk(m_);
        cv_.wait_for(lk, std::chrono::milliseconds(waitMs), [&]
                     { return !pending_.empty() || stopping_; });
        if (pending_.empty())
            return nullptr;

        auto job = pending_.front();
        pending_.pop_front();
        {
            std::lock_guard<std::mutex> jl(job->m); // m_ -> job->m is the allowed order
            job->state = JobState::Running;
        }
        return job;
    }

    void finish(const std::shared_ptr<Job> &job, bool ok, std::string image, std::string error)
    {
        {
            std::lock_guard<std::mutex> jl(job->m);
            job->state = ok ? JobState::Done : JobState::Failed;
            job->image = std::move(image);
            job->error = std::move(error);
            job->finishedAt = Clock::now();
        }
        job->cv.notify_all();
        (ok ? completed_ : failed_)++;
    }

    // Fail everything still waiting so blocked HTTP handlers return promptly.
    void shutdown()
    {
        std::vector<std::shared_ptr<Job>> dropped;
        {
            std::lock_guard<std::mutex> lk(m_);
            stopping_ = true;
            dropped.assign(pending_.begin(), pending_.end());
            pending_.clear();
        }
        for (auto &j : dropped)
            finish(j, false, "", "server shutting down");
        cv_.notify_all();
    }

    unsigned long long completed() const { return completed_; }
    unsigned long long failed() const { return failed_; }

private:
    static constexpr size_t kMaxFinished = 256;

    std::string makeId() // m_ held
    {
        static const char hex[] = "0123456789abcdef";
        std::string id;
        for (int w = 0; w < 2; w++)
        {
            unsigned long long v = rng_();
            for (int i = 0; i < 16; i++)
                id += hex[(v >> (i * 4)) & 15];
        }
        return id;
    }

    // Drop finished jobs nobody collected (older than the TTL, or beyond the cap).
    void prune() // m_ held
    {
        auto now = Clock::now();
        std::vector<std::pair<Clock::time_point, std::string>> finished;

        for (auto it = jobs_.begin(); it != jobs_.end();)
        {
            bool fin;
            Clock::time_point t;
            {
                std::lock_guard<std::mutex> jl(it->second->m);
                fin = it->second->state == JobState::Done || it->second->state == JobState::Failed;
                t = it->second->finishedAt;
            }
            if (fin && now - t > std::chrono::seconds(ttl_))
            {
                it = jobs_.erase(it);
                continue;
            }
            if (fin)
                finished.emplace_back(t, it->first);
            ++it;
        }

        if (finished.size() > kMaxFinished)
        {
            std::sort(finished.begin(), finished.end());
            for (size_t i = 0; i < finished.size() - kMaxFinished; i++)
                jobs_.erase(finished[i].second);
        }
    }

    size_t maxPending_;
    int ttl_;

    std::mutex m_;
    std::condition_variable cv_;
    std::deque<std::shared_ptr<Job>> pending_;
    std::unordered_map<std::string, std::shared_ptr<Job>> jobs_;
    std::mt19937_64 rng_;
    bool stopping_ = false;

    std::atomic<unsigned long long> completed_{0};
    std::atomic<unsigned long long> failed_{0};
};

// ---------------------------------------------------------------------------
// Request parsing / JSON helpers
// ---------------------------------------------------------------------------

static std::string jsonQuote(const std::string &s)
{
    std::string o = "\"";
    for (unsigned char c : s)
    {
        switch (c)
        {
        case '"':
            o += "\\\"";
            break;
        case '\\':
            o += "\\\\";
            break;
        case '\n':
            o += "\\n";
            break;
        case '\r':
            o += "\\r";
            break;
        case '\t':
            o += "\\t";
            break;
        default:
            if (c < 0x20)
            {
                char b[8];
                std::snprintf(b, sizeof b, "\\u%04x", c);
                o += b;
            }
            else
                o += (char)c;
        }
    }
    return o + "\"";
}

static std::string errorJson(const std::string &msg)
{
    return "{\"error\":" + jsonQuote(msg) + "}";
}

// Only plain file names inside <content>/textures - never paths.
static bool validFileName(const std::string &n)
{
    if (n.empty() || n.size() > 64 || n[0] == '.' || n.find("..") != std::string::npos)
        return false;
    for (unsigned char c : n)
        if (!(std::isalnum(c) || c == '.' || c == '_' || c == '-'))
            return false;
    return true;
}

static bool parseHexColor(std::string v, unsigned &out)
{
    if (!v.empty() && v[0] == '#')
        v.erase(0, 1);
    if (v.size() != 6)
        return false;
    char *end = nullptr;
    unsigned long n = std::strtoul(v.c_str(), &end, 16);
    if (*end != '\0')
        return false;
    out = (unsigned)n;
    return true;
}

// Works for ?query=strings and form-encoded POST bodies alike (httplib merges both).
static bool parseRequest(const httplib::Request &req, const Config &cfg, RenderRequest &out, std::string &err)
{
    if (req.has_param("size"))
    {
        std::string v = req.get_param_value("size");
        char *end = nullptr;
        long n = std::strtol(v.c_str(), &end, 10);
        if (v.empty() || *end != '\0' || n < 16 || n > cfg.maxSize)
        {
            err = "size must be an integer between 16 and " + std::to_string(cfg.maxSize);
            return false;
        }
        out.size = (int)n;
    }

    if (req.has_param("quality"))
    {
        std::string v = req.get_param_value("quality");
        if (v == "hq")
            out.hq = true;
        else if (v == "lq")
            out.hq = false;
        else
        {
            err = "quality must be hq or lq";
            return false;
        }
    }

    auto texture = [&](const char *key, std::string &dst) -> bool
    {
        if (!req.has_param(key))
            return true; // keep the default
        std::string v = req.get_param_value(key);
        if (v.empty() || v == "none")
        {
            dst.clear();
            return true;
        }
        if (!validFileName(v))
        {
            err = std::string("invalid ") + key + " file name";
            return false;
        }
        if (!fs::exists(fs::path(cfg.contentDir) / "textures" / v))
        {
            err = std::string(key) + " texture not found: " + v;
            return false;
        }
        dst = v;
        return true;
    };
    if (!texture("face", out.face) || !texture("shirt", out.shirt) || !texture("pants", out.pants))
        return false;

    for (int i = 0; i < RenderRequest::PART_COUNT; i++)
    {
        if (!req.has_param(PART_KEYS[i]))
            continue;
        if (!parseHexColor(req.get_param_value(PART_KEYS[i]), out.colors[i]))
        {
            err = std::string(PART_KEYS[i]) + " must be a colour like F5CD30";
            return false;
        }
    }
    return true;
}

static std::string jobJson(JobQueue &queue, const std::shared_ptr<Job> &job)
{
    JobState st;
    std::string image, error;
    {
        std::lock_guard<std::mutex> lk(job->m);
        st = job->state;
        if (st == JobState::Done)
            image = job->image;
        else if (st == JobState::Failed)
            error = job->error;
    }

    std::string j = "{\"jobId\":" + jsonQuote(job->id) + ",\"status\":\"" + stateName(st) + "\"";
    if (st == JobState::Queued)
        j += ",\"position\":" + std::to_string(queue.position(job)); // job->m released above
    if (st == JobState::Done)
        j += ",\"image\":" + jsonQuote(image);
    if (st == JobState::Failed)
        j += ",\"error\":" + jsonQuote(error);
    return j + "}";
}

static bool authorized(const httplib::Request &req, const std::string &key)
{
    if (req.get_header_value("X-Api-Key") == key)
        return true;
    return req.get_header_value("Authorization") == "Bearer " + key;
}

// ---------------------------------------------------------------------------

static std::atomic<bool> g_stop{false};
extern "C" void onSignal(int) { g_stop = true; }

int main(int argc, char **argv)
{
    Config cfg;
    if (!parseArgs(argc, argv, cfg))
    {
        usage(argv[0]);
        return 1;
    }

    // OpenGL lives on this (the main) thread for the whole run.
    AvatarRenderer renderer;
    std::string err;
    if (!renderer.init(cfg.contentDir, err))
    {
        std::printf("renderer init failed: %s\n", err.c_str());
        return 1;
    }

    JobQueue queue(cfg.maxQueue, cfg.resultTtlSeconds);
    httplib::Server svr;
    svr.set_default_headers({{"Cache-Control", "no-store"}});

        
    svr.set_pre_routing_handler([&](const httplib::Request &req, httplib::Response &res) -> httplib::Server::HandlerResponse
    {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization, X-Api-Key");

        if (req.method == "OPTIONS")
        {
            res.status = 204;
            return httplib::Server::HandlerResponse::Handled;
        }

        if (cfg.apiKey.empty() || req.path == "/health" || req.path == "/" || req.path == "/test" || authorized(req, cfg.apiKey))
            return httplib::Server::HandlerResponse::Unhandled;

        res.status = 401;
        res.set_content(errorJson("missing or wrong API key"), "application/json");
        return httplib::Server::HandlerResponse::Handled;
    });


    // GET /  (or /test)  -> the test page, <content>/test.html. Read from disk on every request,
    // so you can edit it without restarting. Not behind the API key: the page asks for the key itself.
    auto servePage = [&](const httplib::Request &, httplib::Response &res)
    {
        std::ifstream f(fs::path(cfg.contentDir) / "test.html", std::ios::binary);
        if (!f)
        {
            res.status = 404;
            res.set_content("test.html not found in the content folder", "text/plain");
            return;
        }
        std::string html((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        res.set_content(std::move(html), "text/html; charset=utf-8");
    };
    svr.Get("/", servePage);
    svr.Get("/test", servePage);

    // GET /textures -> {"files":["face.png","shirt1.png"]}  the names face/shirt/pants accept
    svr.Get("/textures", [&](const httplib::Request &, httplib::Response &res)
            {
        std::vector<std::string> names;
        std::error_code ec;
        for (const auto &entry : fs::directory_iterator(fs::path(cfg.contentDir) / "textures", ec))
        {
            std::string n = entry.path().filename().string();
            if (entry.is_regular_file(ec) && validFileName(n))
                names.push_back(n);
        }
        std::sort(names.begin(), names.end());

        std::string j = "{\"files\":[";
        for (size_t i = 0; i < names.size(); i++)
            j += (i ? "," : "") + jsonQuote(names[i]);
        res.set_content(j + "]}", "application/json"); });

    // GET|POST /render?size=512&shirt=a.png&head=F5CD30
    //   Waits its turn in the queue and answers with the base64 PNG as text/plain.
    //   If the wait would outlive Cloudflare's timeout it answers 202 with a jobId
    //   instead, and the render carries on - poll GET /jobs/<id>.
    auto renderHandler = [&](const httplib::Request &req, httplib::Response &res)
    {
        RenderRequest rr;
        std::string perr;
        if (!parseRequest(req, cfg, rr, perr))
        {
            res.status = 400;
            res.set_content(errorJson(perr), "application/json");
            return;
        }

        auto job = queue.submit(rr);
        if (!job)
        {
            res.status = 503;
            res.set_header("Retry-After", "5");
            res.set_content(errorJson("render queue is full, try again shortly"), "application/json");
            return;
        }

        bool finished;
        {
            std::unique_lock<std::mutex> lk(job->m);
            finished = job->cv.wait_for(lk, std::chrono::seconds(cfg.syncWaitSeconds), [&]
                                        { return job->state == JobState::Done || job->state == JobState::Failed; });
        }

        if (!finished)
        {
            res.status = 202;
            res.set_header("Location", "/jobs/" + job->id);
            res.set_content(jobJson(queue, job), "application/json");
            return;
        }

        bool ok;
        std::string image, error;
        {
            std::lock_guard<std::mutex> lk(job->m);
            ok = job->state == JobState::Done;
            image = std::move(job->image);
            error = job->error;
        }
        queue.forget(job->id); // delivered - no need to keep it around

        if (ok)
            res.set_content(std::move(image), "text/plain");
        else
        {
            res.status = 500;
            res.set_content(errorJson(error), "application/json");
        }
    };
    svr.Get("/render", renderHandler);
    svr.Post("/render", renderHandler);

    // POST /jobs?...   -> 202 {"jobId":"..","status":"queued","position":N}
    svr.Post("/jobs", [&](const httplib::Request &req, httplib::Response &res)
             {
        RenderRequest rr;
        std::string perr;
        if (!parseRequest(req, cfg, rr, perr))
        {
            res.status = 400;
            res.set_content(errorJson(perr), "application/json");
            return;
        }
        auto job = queue.submit(rr);
        if (!job)
        {
            res.status = 503;
            res.set_header("Retry-After", "5");
            res.set_content(errorJson("render queue is full, try again shortly"), "application/json");
            return;
        }
        res.status = 202;
        res.set_header("Location", "/jobs/" + job->id);
        res.set_content(jobJson(queue, job), "application/json"); });

    // GET /jobs/<id>   -> {"jobId":"..","status":"queued|running|done|failed", ...}
    svr.Get(R"(/jobs/([0-9a-f]{32}))", [&](const httplib::Request &req, httplib::Response &res)
            {
        auto job = queue.find(req.matches[1]);
        if (!job)
        {
            res.status = 404;
            res.set_content(errorJson("unknown or expired job"), "application/json");
            return;
        }
        res.set_content(jobJson(queue, job), "application/json"); });

    if (!svr.bind_to_port(cfg.host, cfg.port))
    {
        std::printf("could not bind %s:%d\n", cfg.host.c_str(), cfg.port);
        return 1;
    }
    std::thread http([&]
                     { svr.listen_after_bind(); });

    std::signal(SIGINT, onSignal);
    std::signal(SIGTERM, onSignal);

    std::printf("AvatarRenderer listening on http://%s:%d  (queue limit %zu, auth %s)\n",
                cfg.host.c_str(), cfg.port, cfg.maxQueue, cfg.apiKey.empty() ? "off" : "on");

    // ---- render loop: one job at a time, in arrival order --------------------
    while (!g_stop)
    {
        renderer.pump();
        auto job = queue.next(100);
        if (!job)
            continue;

        auto t0 = Clock::now();
        std::string image, jerr;
        bool ok = false;
        try
        {
            ok = renderer.render(job->req, image, jerr);
        }
        catch (const std::exception &e)
        {
            jerr = e.what();
        }
        long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - t0).count();

        std::printf("[job %.8s] %s %dpx %s %lldms%s%s\n", job->id.c_str(), ok ? "ok" : "FAILED",
                    job->req.size, job->req.hq ? "hq" : "lq", ms, ok ? "" : " - ", ok ? "" : jerr.c_str());

        queue.finish(job, ok, std::move(image), std::move(jerr));
    }

    std::printf("shutting down...\n");
    queue.shutdown();
    svr.stop();
    http.join();
    return 0;
}

# Native login compatibility fix — 2026-10-10

Use auth.js from THIS directory, not the earlier auth-session-validation.js.
Back up the deployed modules/auth.js, replace it with this auth.js, and restart the existing website Node service using its normal process manager. Keep modules/rateLimiter.js unchanged. No database migration or password reset is required. The provided Downloads/site (1) files were not modified.

POST /api/login-user, Content-Type: application/json:
- Password login: {"username":"ACCOUNT","password":"PASSWORD"}. Captcha is optional; a supplied proof must verify. Existing five-attempt/15-minute limiter remains.
- Session validation: {"token":"COOKIE_TOKEN"}, with no username/password. Only a stored 64-hex session authenticates. Success returns {message,cookieToken,user}; invalid/expired session returns 401; banned accounts remain rejected. Validation does not rotate the token.
- Actual wrong credentials return 401; malformed credentials 400; rate limit 429. Password hashing is unchanged.

Registration continues requiring captcha. The native signup flow therefore still needs a separate compatible registration solution. Website signup bugs identified in FINDINGS.md were not changed.

Login re-reads accounts after asynchronous bcrypt verification before storing a new token, avoiding overwriting concurrent profile updates with the old login snapshot. This remains the site's single-process JSON database, not a new database architecture. Session expiry is only whatever the existing backend enforces; no expiry mechanism is invented here.

Verified: 12 backend checks with the actual supplied rate limiter and a disposable database; original game SignInScreen -> LinkAccountScreen -> authenticated menu passed against the candidate route (UserId 7). Turnstile verification was mocked in backend tests only. No production credentials, database, or hosted endpoint was modified/tested. Game client rebuilt successfully.

The live service will continue rejecting native login until your friend DEPLOYS this module. Use HTTPS for production credentials. Client now distinguishes captcha-required/malformed requests from incorrect passwords.

#pragma once
#include <windows.h>
#include <string>
struct AvatarPromptState { HWND edit=nullptr;bool done=false,accepted=false,password=false;unsigned limit=64;std::wstring value; };
static LRESULT CALLBACK avatarPromptProc(HWND window,UINT message,WPARAM w,LPARAM l) {
    auto state=reinterpret_cast<AvatarPromptState*>(GetWindowLongPtrW(window,GWLP_USERDATA));
    if(message==WM_CREATE){state=static_cast<AvatarPromptState*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);SetWindowLongPtrW(window,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(state));
        state->edit=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",state->value.c_str(),WS_CHILD|WS_VISIBLE|WS_TABSTOP|ES_AUTOHSCROLL|(state->password?ES_PASSWORD:0),15,15,400,28,window,(HMENU)100,GetModuleHandleW(nullptr),nullptr);
        SendMessageW(state->edit,EM_SETLIMITTEXT,state->limit,0);
        CreateWindowW(L"BUTTON",L"Save",WS_CHILD|WS_VISIBLE|WS_TABSTOP,220,60,90,30,window,(HMENU)IDOK,GetModuleHandleW(nullptr),nullptr);
        CreateWindowW(L"BUTTON",L"Cancel",WS_CHILD|WS_VISIBLE|WS_TABSTOP,325,60,90,30,window,(HMENU)IDCANCEL,GetModuleHandleW(nullptr),nullptr);return 0;}
    if(state && message==WM_COMMAND && (LOWORD(w)==IDOK || LOWORD(w)==IDCANCEL)) {
        state->accepted=LOWORD(w)==IDOK;wchar_t text[129]={};GetWindowTextW(state->edit,text,129);state->value=text;SecureZeroMemory(text,sizeof(text));state->done=true;return 0;}
    if(state && message==WM_CLOSE){state->done=true;return 0;}
    return DefWindowProcW(window,message,w,l);
}
static std::wstring avatarTextPrompt(const std::wstring& title,const std::wstring& initial,bool password=false,unsigned limit=64) {
    WNDCLASSW type={};type.hInstance=GetModuleHandleW(nullptr);type.lpszClassName=L"Reboot16AvatarText";type.lpfnWndProc=avatarPromptProc;type.hCursor=LoadCursorW(nullptr,MAKEINTRESOURCEW(32512));type.hbrBackground=(HBRUSH)(COLOR_WINDOW+1);RegisterClassW(&type);
    AvatarPromptState state;state.value=initial;state.password=password;state.limit=limit;HWND owner=GetActiveWindow();
    HWND window=CreateWindowExW(WS_EX_DLGMODALFRAME,L"Reboot16AvatarText",title.c_str(),WS_CAPTION|WS_SYSMENU|WS_VISIBLE,300,220,450,135,owner,nullptr,type.hInstance,&state);
    if(!window)return L"";EnableWindow(owner,FALSE);SetFocus(state.edit);MSG message;
    while(!state.done && GetMessageW(&message,nullptr,0,0)>0){
        if(message.message==WM_KEYDOWN && message.wParam==VK_RETURN){SendMessageW(window,WM_COMMAND,IDOK,0);continue;}
        if(message.message==WM_KEYDOWN && message.wParam==VK_ESCAPE){state.done=true;continue;}
        if(!IsDialogMessageW(window,&message)){TranslateMessage(&message);DispatchMessageW(&message);}
    }
    DestroyWindow(window);EnableWindow(owner,TRUE);SetActiveWindow(owner);return state.accepted?state.value:L"";
}

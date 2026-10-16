/* Test-only independent client launch against the real native NexaCloud loop. */
static void uia_client_log(HANDLE pipe) {
    DWORD available=0;
    while (PeekNamedPipe(pipe,NULL,0,NULL,&available,NULL) && available) {
        char bytes[4096]; DWORD read=0;
        if (!ReadFile(pipe,bytes,available<sizeof(bytes) ? available : (DWORD)sizeof(bytes),&read,NULL) || !read) break;
        fwrite(bytes,1,read,stderr);
    }
}
static int uia_smoke(nx_window *window,cloud_scene *scene,nx_renderer_driver *renderer,const char *client_path) {
    if (!frame(window,scene,renderer)) return 0;
    nx_window_info info; if (nx_window_get_info(window,&info)!=NX_OK) return 0;
    fprintf(stderr,"NexaCloud UIA logical client %.0fx%.0f scale %.2f\n",info.width,info.height,info.scale);
    WCHAR client[2048],command[4096];
    /* main's CRT argv is in the Windows process code page. */
    if (!MultiByteToWideChar(CP_ACP,0,client_path,-1,client,2048) ||
        swprintf_s(command,4096,L"\"%ls\" --cloud %llu",client,
            (unsigned long long)(UINT_PTR)nx_window_native_hwnd(window))<0) return 0;
    SECURITY_ATTRIBUTES security={sizeof(security),NULL,TRUE};
    HANDLE read=NULL,write=NULL,input=INVALID_HANDLE_VALUE;
    if (!CreatePipe(&read,&write,&security,0)) return 0;
    if (!SetHandleInformation(read,HANDLE_FLAG_INHERIT,0)) { CloseHandle(read); CloseHandle(write); return 0; }
    input=CreateFileW(L"NUL",GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,&security,OPEN_EXISTING,0,NULL);
    if (input==INVALID_HANDLE_VALUE) { CloseHandle(read); CloseHandle(write); return 0; }
    STARTUPINFOW startup={0}; startup.cb=sizeof(startup); startup.dwFlags=STARTF_USESTDHANDLES;
    startup.hStdInput=input; startup.hStdOutput=startup.hStdError=write;
    PROCESS_INFORMATION process={0};
    int created=CreateProcessW(client,command,NULL,NULL,TRUE,CREATE_NO_WINDOW,NULL,NULL,&startup,&process)!=0;
    CloseHandle(input); CloseHandle(write);
    if (!created) { CloseHandle(read); return 0; }
    CloseHandle(process.hThread);
    int okay=1; ULONGLONG deadline=GetTickCount64()+90000;
    while (WaitForSingleObject(process.hProcess,0)==WAIT_TIMEOUT) {
        uia_client_log(read);
        if (GetTickCount64()>=deadline || !frame(window,scene,renderer)) {
            /* Only this test's explicitly created child process is terminated. */
            TerminateProcess(process.hProcess,1); okay=0; break;
        }
        Sleep(1);
    }
    uia_client_log(read);
    DWORD status=1; if (!GetExitCodeProcess(process.hProcess,&status) || status) okay=0;
    CloseHandle(process.hProcess); CloseHandle(read);
    return okay;
}

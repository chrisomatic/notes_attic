static void start_process(char* command,bool show_window = false)
{

    STARTUPINFO si = {0};
    PROCESS_INFORMATION pi = {0};

    si.cb = sizeof(si);

	DWORD window_creation_flags = 0;

    if(show_window)
		window_creation_flags |= CREATE_NEW_CONSOLE;

	int result = CreateProcess(
			NULL
			, command
			, NULL
			, NULL
			, FALSE
			, window_creation_flags
            ,NULL
            ,NULL
            ,&si
            ,&pi
        );

    if(!result)
    {
        printf( "CreateProcess failed (%d).\n",GetLastError());
        return;
    }

    //WaitForSingleObject(pi.hProcess, INFINITE);

    //CloseHandle( pi.hProcess);
    //CloseHandle( pi.hThread);
}

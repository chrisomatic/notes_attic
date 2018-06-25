static int start_process(char* command,bool show_window = false, bool wait = false)
{

    STARTUPINFOA si = {0};
    PROCESS_INFORMATION pi = {0};

    si.cb = sizeof(si);

	DWORD window_creation_flags = 0;

	if (show_window)
		window_creation_flags |= CREATE_NEW_CONSOLE;
	else
		window_creation_flags |= CREATE_NO_WINDOW;

	int result = CreateProcessA(
			NULL
			, (LPSTR)command
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
        printf("CreateProcess failed (%d).\n",GetLastError());
        return 1;
    }

    if(wait)
    {
        WaitForSingleObject(pi.hProcess, INFINITE);

		DWORD exitCode;
		result = GetExitCodeProcess(pi.hProcess, &exitCode);

		if (!result)
		{
			printf("GetExitCodeProcess Failed (%d).\n", GetLastError());
			return 2;
		}

        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);

		return exitCode;
    }

	return 0;
}

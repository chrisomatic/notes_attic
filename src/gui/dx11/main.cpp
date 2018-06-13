#pragma comment(linker, "/STACK:3000000")
#pragma comment(linker, "/HEAP:2000000")

#define VERSION_MAJOR 1
#define VERSION_MINOR 5

#define MAX_EMAILS 1000000
#define FIELD_KEY_MAX 100
#define FIELD_VAL_MAX 10000
#define FIELD_BODY_MAX 100000
#define BIN_LENGTH 32

#include <imgui\imgui.h>
#include <imgui\imgui_freetype.h>
#include <Shlobj.h>
#include "imgui_impl_dx11.h"
#include <shellapi.h>
#include <d3d11.h>
#define DIRECTINPUT_VERSION 0x0800
#include <Strsafe.h>
#include <dinput.h>
#include <tchar.h>
#include <dirent.h>
#include <time.h>
#include <thread>
#include "process.h"
#include "resource.h"

typedef enum
{
    NOTES_DATE,
    TO,
    FROM,
    SUBJECT,
    CC,
    FORWARDED,
    BODY
} EmailHeaderField;

#include "quicksort.h"

static int* filtered_indicies;
static int* sorted_indicies;

static int num_emails = 0;
static int num_emails_filtered = 0;

static RECT window_rect;
static int window_width;
static int window_height;
static int emails_window_width = 420;

static int  sort_field = 0;
static int  sort_dir = 1;

static int  selected_font_index = 0;
static int  selected_style_index = 1;

static ImVec4 header_color;

static char filter_all[FIELD_VAL_MAX] = {0};
static char filter_to[FIELD_VAL_MAX] = {0};
static char filter_cc[FIELD_VAL_MAX] = {0};
static char filter_from[FIELD_VAL_MAX] = {0};
static char filter_date[FIELD_VAL_MAX] = {0};
static char filter_forwarded[FIELD_VAL_MAX] = {0};
static char filter_subject[FIELD_VAL_MAX] = {0};
static char filter_body[FIELD_VAL_MAX] = {0};

static bool only_attachments = false;
static bool orient_right = false;
static bool is_automatic_downloading_enabled = false;
static const char* dow_items[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
static int  automatic_downloading_freq = 1;
static int  automatic_downloading_pref_day = 1;
static int  automatic_downloading_pref_dow = 0;
static int  automatic_downloading_pref_hour = 8;
static int  automatic_downloading_pref_min = 0;

static char xls_program_path[MAX_PATH]  = {0};
static char doc_program_path[MAX_PATH]  = {0};
static char csv_program_path[MAX_PATH]  = {0};
static char txt_program_path[MAX_PATH]  = {0};
static char pdf_program_path[MAX_PATH] = {0};
static char img_program_path[MAX_PATH] = {0};
static char misc_program_path[MAX_PATH] = {0};
static char export_program_path[MAX_PATH] = {0};

static bool xls_override  = false;
static bool doc_override  = false;
static bool csv_override  = false;
static bool txt_override  = false;
static bool pdf_override  = false;
static bool img_override  = false;
static bool misc_override = true;

static char root_dir[MAX_PATH] = {0};
static bool show_settings_window = false;
static bool set_scroll_here  = false;
static bool cache_is_updated = false;

static char notes_password[100] = {0};
static char config_path[MAX_PATH];
static char* download_datetime;
static char* get_app_directory();

static HMENU hmenu;
static GUID guid;

static MSG msg;

static int selected_email_index = -1;
static int prev_selected_email_index = -1;
static int pass_flags;
static ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse;

// Data
static ID3D11Device*            g_pd3dDevice = NULL;
static ID3D11DeviceContext*     g_pd3dDeviceContext = NULL;
static IDXGISwapChain*          g_pSwapChain = NULL;
static ID3D11RenderTargetView*  g_mainRenderTargetView = NULL;

void CreateRenderTarget()
{
    DXGI_SWAP_CHAIN_DESC sd;
    g_pSwapChain->GetDesc(&sd);

    // Create the render target
    ID3D11Texture2D* pBackBuffer;
    D3D11_RENDER_TARGET_VIEW_DESC render_target_view_desc;
    ZeroMemory(&render_target_view_desc, sizeof(render_target_view_desc));
    render_target_view_desc.Format = sd.BufferDesc.Format;
    render_target_view_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, &render_target_view_desc, &g_mainRenderTargetView);
    pBackBuffer->Release();
}

void CleanupRenderTarget()
{
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = NULL; }
}

HRESULT CreateDeviceD3D(HWND hWnd)
{
    // Setup swap chain
    DXGI_SWAP_CHAIN_DESC sd;
    {
        ZeroMemory(&sd, sizeof(sd));
        sd.BufferCount = 2;
        sd.BufferDesc.Width = 0;
        sd.BufferDesc.Height = 0;
        sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.BufferDesc.RefreshRate.Numerator = 60;
        sd.BufferDesc.RefreshRate.Denominator = 1;
        sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.OutputWindow = hWnd;
        sd.SampleDesc.Count = 1;
        sd.SampleDesc.Quality = 0;
        sd.Windowed = TRUE;
        sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    }

    UINT createDeviceFlags = 0;
    //createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
    if (D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext) != S_OK)
        return E_FAIL;

    CreateRenderTarget();

    return S_OK;
}

void CleanupDeviceD3D()
{
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = NULL; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = NULL; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = NULL; }
}

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
	case WM_CREATE:
		ShowWindow(hWnd, SW_HIDE);
		hmenu = CreatePopupMenu();
		AppendMenu(hmenu, MF_STRING, ID_TRAY_SHOW, TEXT("Show UI"));
		AppendMenu(hmenu, MF_STRING, ID_TRAY_EXIT, TEXT("Quit"));

		break;
	case WM_CLOSE:
		ShowWindow(hWnd, SW_HIDE);
		return 0;
	case WM_SYSICON:
	{
		switch (wParam)
		{
		case ID_TRAY_APP_ICON:
			SetForegroundWindow(hWnd);
			break;
		}
		if (lParam == WM_LBUTTONUP)
			ShowWindow(hWnd, SW_SHOW);
		else if (lParam == WM_RBUTTONDOWN)
		{
			POINT curPoint;
			GetCursorPos(&curPoint);
			SetForegroundWindow(hWnd);

			UINT clicked = TrackPopupMenu(hmenu, TPM_RETURNCMD | TPM_NONOTIFY, curPoint.x, curPoint.y, 0, hWnd, NULL);
			SendMessage(hWnd, WM_NULL, 0, 0); // send benign message to window to make sure the menu goes away.

            if (clicked == ID_TRAY_SHOW)
            {
                ShowWindow(hWnd, SW_SHOW);
            }
			else if (clicked == ID_TRAY_EXIT)
			{
				NOTIFYICONDATA nid = { 0 };

				nid.cbSize = sizeof(NOTIFYICONDATA);
				nid.uFlags = NIF_GUID;
				nid.guidItem = guid;

				Shell_NotifyIcon(NIM_DELETE, &nid);

				PostQuitMessage(0);
			}
		}
	}
	break;
    case WM_SIZE:

        if (g_pd3dDevice != NULL && wParam != SIZE_MINIMIZED)
        {
            GetClientRect(hWnd, &window_rect);

            window_width = window_rect.right - window_rect.left;
            window_height = window_rect.bottom - window_rect.top;

            ImGui_ImplDX11_InvalidateDeviceObjects();
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
            CreateRenderTarget();
            ImGui_ImplDX11_CreateDeviceObjects();
        }
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
            return 0;
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

static const char* default_root_dir  = "C:\\email_archive";
static const char* default_xls_path  = "C:\\Program Files\\Microsoft Office\\OFFICE14\\EXCEL.EXE";
static const char* default_doc_path  = "C:\\Program Files\\Microsoft Office\\OFFICE14\\WINWORD.EXE";
static const char* default_csv_path  = "C:\\Windows\\system32\\notepad.exe";
static const char* default_txt_path  = "C:\\Windows\\system32\\notepad.exe";
static const char* default_pdf_path  = "C:\\Program Files\\Adobe\\Acrobat Reader DC\\Reader\\AcroRd32.exe";
static const char* default_img_path  = "C:\\Windows\\system32\\rundll32.exe \"C:\\Program Files\\Windows Photo Viewer\\PhotoViewer.dll\", ImageView_Fullscreen";
static const char* default_misc_path = "C:\\Windows\\system32\\notepad.exe";

static const char* default_export_program_path = "exportnotes.exe";

static char cache_file_path[MAX_PATH];
static char cache_file_path_temp[MAX_PATH];
static char cache_file_path_del[MAX_PATH];
static char config_file_path[MAX_PATH];
static char bin_file_path[MAX_PATH];

typedef struct
{
    char* file_path;
	char* to;
    char* to_formatted;
	char* cc;
	char* from;
    char* from_formatted;
	char* forwarded;
	char* date;
    char* date_formatted;
	char* subject;
    char* body;
	char** attachments;
    char** attachments_ext;
	int   attachment_count;
} NotesEmail;

static NotesEmail emails[MAX_EMAILS];
static NotesEmail curr_email;

static void parse_email(const char* file_path,int index);
static void get_emails_direct(const char* directory_path,FILE* fp, int* temp_num_emails);
static void write_direct_to_cache(const char* directory_path);
static void get_emails(const char* directory_path);
static void delete_email(int index);
static void export_emails(bool show, bool wait);
static void export_emails_with_pass(bool show,bool wait, const char* pass);
static void export_emails_with_pass_background(bool show,bool wait, const char* pass);
static void download_emails_and_refresh();
static void display_main_menu();
static void apply_filters();
static void apply_sorting();
static void reset_and_load_emails();
static void load_emails();
static void store_cache();
static bool load_cache();
static void load_config();
static void store_config();
static void load_pass();
static void store_pass();
static void clear_email_memory();
static bool str_contains(char* base, char* search_pattern,bool case_sensitive);
static int str_replace(char* base, char* search_pattern, char* replacement, char* ret_string);
static bool str_startswith(char* base, char* search_pattern);
static void str_tolower(char *str);
static char* str_getext(const char* file_path);
static char* dt_convert_to_sortable_format(const char* date);
static void start_default_process(const char* path);
static bool show_style_selector(const char* label);
static void load_style_monochrome();
static void load_style_ecstacy();
static void load_style_minimal();
static void load_style_gruvbox();
static void load_style_gruvbox_light();
static void load_style_gumby();
static void draw_filters_email_list(int x);
static void draw_header_body_attachments(int x);
static char* get_current_datetime(struct tm &tstruct);
static void get_next_download_datetime(char* download_datetime);
static bool time_to_download();
static bool directory_exists(char* szPath);
static bool file_exists(char* file);
static char* get_app_full_file_path(char* relative_file_path);
static char* encrypt_password(char* password);
static char* decrypt_password(char* encrypted_password);

int WINAPI WinMain(HINSTANCE hinstance, HINSTANCE hprevinstance, LPSTR lpcmdline, int nshowcmd)
//int main(int, char**)
{
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_PROFILE, NULL, 0, (LPSTR)config_path)))
		strncat(config_path, "\\AppData\\Local\\Attic\\",22);
	else
		strncpy(config_path, "C:\\Attic_Config\\",17);

	//FreeConsole();
	CreateDirectoryA((LPCSTR)config_path, NULL);

	int config_path_length = strlen(config_path);

	// set cache and config file locations
	strncpy(cache_file_path, config_path, config_path_length);
	strcat(cache_file_path, "attic.data");

	strncpy(cache_file_path_temp, config_path, config_path_length);
	strcat(cache_file_path_temp, "attic_temp.data");

	strncpy(cache_file_path_del, config_path, config_path_length);
	strcat(cache_file_path_del, "attic_del.data");

	strncpy(config_file_path, config_path, config_path_length);
	strcat(config_file_path, "attic.cfg");

	strncpy(bin_file_path, config_path, config_path_length);
	strcat(bin_file_path, "attic.bin");

	window_width = 1280;
	window_height = 800;
    
	HRESULT hCreateGuid = CoCreateGuid(&guid);

    // Create application window
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(NULL), NULL, LoadCursor(NULL, IDC_ARROW), NULL, NULL, _T("Notes Attic"), NULL };
	wc.hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_ICON1));
	wc.hIconSm = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_ICON1));

    RegisterClassEx(&wc);
    HWND hwnd = CreateWindow(_T("Notes Attic"), _T("Notes Attic"), WS_OVERLAPPEDWINDOW, 100, 100, window_width, window_height, NULL, NULL, wc.hInstance, NULL);

    // Initialize Direct3D
    if (CreateDeviceD3D(hwnd) < 0)
    {
        CleanupDeviceD3D();
        UnregisterClass(_T("Notes Attic"), wc.hInstance);
        return 1;
    }
    
    // try to init system tray icon
    NOTIFYICONDATA nid = {0};

	nid.cbSize = sizeof(NOTIFYICONDATA);
	nid.hWnd = hwnd;
	nid.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE | NIF_SHOWTIP | NIF_GUID;
	nid.guidItem = guid;
	nid.uCallbackMessage = WM_SYSICON; //Set up our invented Windows Message 
	nid.hIcon = LoadIcon(wc.hInstance, MAKEINTRESOURCE(IDI_ICON1));
	WCHAR* message = TEXT("Notes Attic\nCompanion App for IBM Notes");
	StringCchCopyW(nid.szTip, 50, message);
	nid.uVersion = NOTIFYICON_VERSION_4;
	Shell_NotifyIcon(NIM_ADD, &nid);

	bool success = Shell_NotifyIcon(NIM_SETVERSION, &nid); 
	if (!success)
		printf("Error adding icon!");

    // Show the window
    ShowWindow(hwnd, SW_SHOWDEFAULT);
    UpdateWindow(hwnd);

    // Setup ImGui binding
    ImGui_ImplDX11_Init(hwnd, g_pd3dDevice, g_pd3dDeviceContext);

	ImFontConfig config;
	config.OversampleH = 3;
	config.OversampleV = 1;
	config.GlyphExtraSpacing.x = 1.0f;

    int num_fonts = 1;
    ImGuiIO& io = ImGui::GetIO();

	char full_path_fonts[MAX_PATH] = { 0 };
	strcpy(full_path_fonts, get_app_full_file_path("fonts\\"));

	if (directory_exists(full_path_fonts))
	{
        bool font_loaded = false;

		char font_pt_sans[MAX_PATH] = { 0 };
        char font_karla[MAX_PATH] = { 0 }; 
        char font_inconsolata[MAX_PATH] = { 0 };
        char font_ubuntu[MAX_PATH] = { 0 };

		char* full_path_pt_sans = get_app_full_file_path("fonts\\PT_Sans.ttf");
		strncpy(font_pt_sans, full_path_pt_sans, strlen(full_path_pt_sans));

		char* full_path_karla = get_app_full_file_path("fonts\\Karla.ttf");
		strncpy(font_karla, full_path_karla, strlen(full_path_karla));

		char* full_path_inconsolata = get_app_full_file_path("fonts\\Inconsolata.ttf");
		strncpy(font_inconsolata, full_path_inconsolata, strlen(full_path_inconsolata));

		char* full_path_ubuntu = get_app_full_file_path("fonts\\Ubuntu.ttf");
		strncpy(font_ubuntu, full_path_ubuntu, strlen(full_path_ubuntu));
		
		if (file_exists(font_pt_sans))     {io.Fonts->AddFontFromFileTTF(font_pt_sans    ,16.0f); font_loaded = true;}
		if (file_exists(font_karla))       {io.Fonts->AddFontFromFileTTF(font_karla      ,16.0f); font_loaded = true;}
		if (file_exists(font_inconsolata)) {io.Fonts->AddFontFromFileTTF(font_inconsolata,16.0f); font_loaded = true;}
		if (file_exists(font_ubuntu))      {io.Fonts->AddFontFromFileTTF(font_ubuntu     ,16.0f); font_loaded = true;}

        if(font_loaded)
            num_fonts = 4; 
        else
            io.Fonts->AddFontDefault();
	}
	else
	{
		io.Fonts->AddFontDefault();
	}
	
	unsigned int flags = ImGuiFreeType::ForceAutoHint;
	ImGuiFreeType::BuildFontAtlas(io.Fonts, flags);

    // load defaults
    strncpy(root_dir,default_root_dir,strlen(root_dir));
    strncpy(xls_program_path,default_xls_path, strlen(default_xls_path));
    strncpy(doc_program_path,default_doc_path, strlen(default_doc_path));
    strncpy(csv_program_path,default_csv_path, strlen(default_csv_path));
    strncpy(txt_program_path,default_txt_path, strlen(default_txt_path));
    strncpy(pdf_program_path,default_pdf_path, strlen(default_pdf_path));
    strncpy(img_program_path,default_img_path, strlen(default_img_path));
    strncpy(misc_program_path,default_misc_path, strlen(default_misc_path));
    strncpy(export_program_path,default_export_program_path, strlen(default_export_program_path));

    load_config();
    load_pass();

	// Setup style
    if(selected_font_index > num_fonts - 1)
        selected_font_index = 0;

	io.FontDefault = io.Fonts->Fonts[selected_font_index];

	switch (selected_style_index)
	{
	case 0: ImGui::StyleColorsClassic(); header_color = ImVec4(0.400f,0.200f,0.800f,1.000f); break;
	case 1: ImGui::StyleColorsDark();    header_color = ImVec4(0.400f,0.500f,0.700f,1.000f); break;
	case 2: ImGui::StyleColorsLight();   header_color = ImVec4(0.200f,0.400f,0.900f,1.000f); break;
	case 3: load_style_monochrome();     header_color = ImVec4(0.070f,0.340f,0.340f,1.000f); break;
	case 4: load_style_ecstacy();        header_color = ImVec4(1.000f,1.000f,1.000f,1.000f); break;
	case 5: load_style_minimal();        header_color = ImVec4(0.400f,0.400f,0.400f,1.000f); break;
	case 6: load_style_gruvbox();        header_color = ImVec4(0.400f,0.400f,0.400f,1.000f); break;
	case 7: load_style_gruvbox_light();  header_color = ImVec4(0.400f,0.400f,0.400f,1.000f); break;
	case 8: load_style_gumby();          header_color = ImVec4(0.400f,0.400f,0.400f,1.000f); break;
	}

	ImGuiStyle& style = ImGui::GetStyle();
	style.WindowRounding = 0;
	style.FrameRounding = 3;

    // try to load cache file
    if(!load_cache())
        write_direct_to_cache(root_dir);

    apply_filters();
    apply_sorting();

    ImVec4 clear_color = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);

    // set initial download_datetime
    download_datetime = (char*)calloc(20, sizeof(char));
    get_next_download_datetime(download_datetime);

	// start thread the checks for downloading.
	std::thread t1(download_emails_and_refresh);

    // Main loop
    ZeroMemory(&msg, sizeof(msg));
    while (msg.message != WM_QUIT)
    {
        if (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            continue;
        }

		if (IsIconic(hwnd) || !IsWindowVisible(hwnd))
		{
			// reduce cpu usage when minimized
			Sleep(1);
			continue;
		}

        if(cache_is_updated)
        {
            cache_is_updated = false;
            
            clear_email_memory();
            num_emails = 0;

            load_cache();

            filtered_indicies = (int*)malloc(num_emails*sizeof(int));
            sorted_indicies   = (int*)malloc(num_emails*sizeof(int));

            apply_filters();
            apply_sorting();
        }

        ImGui_ImplDX11_NewFrame();

		display_main_menu();

        // load email if needed
		if (selected_email_index > -1)
		{
			if (selected_email_index != prev_selected_email_index)
			{
				// parse and load email body
                NotesEmail e = emails[filtered_indicies[sorted_indicies[selected_email_index]]];

				curr_email.attachments = e.attachments;
				curr_email.attachments_ext = e.attachments_ext;

				// point current email at selected one
                curr_email.to = e.to_formatted;
                curr_email.cc = e.cc;
                curr_email.from = e.from_formatted;
                curr_email.forwarded = e.forwarded;
                curr_email.date = e.date;
                curr_email.subject = e.subject;
				curr_email.attachment_count = e.attachment_count;
                curr_email.body = e.body;

				prev_selected_email_index = selected_email_index;
			}
		}


        if(orient_right)
        {
            draw_header_body_attachments(0);
            draw_filters_email_list(window_width - emails_window_width);
        }
        else
        {
            draw_filters_email_list(0);
            draw_header_body_attachments(emails_window_width);
        }

        int settings_init_width = 400;
        int settings_init_height = 500;

		if (show_settings_window)
		{
            ImGui::OpenPopup("Settings##PopUp");

			ImGui::SetNextWindowPos(ImVec2((window_width - settings_init_width) / 2.0f, (window_height - settings_init_height) / 2.0f), ImGuiCond_Always);
			ImGui::SetNextWindowSize(ImVec2(settings_init_width, settings_init_height), ImGuiCond_FirstUseEver);
		}

        if(ImGui::BeginPopupModal("Settings##PopUp", NULL, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::Text("Email Root Directory");
            ImGui::PushItemWidth(-1);
            ImGui::InputText("##rootDirectory", root_dir, MAX_PATH); 
            ImGui::Text("Export Program Path");
            ImGui::InputText("##exportprogrampath",export_program_path,MAX_PATH);
            ImGui::PopItemWidth();

            ImGui::Separator();
            ImGui::PushItemWidth(-1);

			ImGui::Checkbox("Override##0",&xls_override); ImGui::SameLine(150);ImGui::Text("XLS/XLSX/XLSM Program Path");
            if(xls_override) ImGui::InputText("##xlsprogrampath",xls_program_path,MAX_PATH);
			ImGui::Checkbox("Override##1",&doc_override); ImGui::SameLine(150);ImGui::Text("DOC/DOCX Program Path");
            if(doc_override) ImGui::InputText("##docprogrampath",doc_program_path,MAX_PATH);
			ImGui::Checkbox("Override##2",&csv_override); ImGui::SameLine(150);ImGui::Text("CSV Program Path");
            if(csv_override) ImGui::InputText("##csvprogrampath",csv_program_path,MAX_PATH);
			ImGui::Checkbox("Override##3",&txt_override); ImGui::SameLine(150);ImGui::Text("TXT Program Path");
            if(txt_override) ImGui::InputText("##txtprogrampath",txt_program_path,MAX_PATH);
			ImGui::Checkbox("Override##4",&pdf_override); ImGui::SameLine(150);ImGui::Text("PDF Program Path");
            if(pdf_override) ImGui::InputText("##pdfprogrampath",pdf_program_path,MAX_PATH);
			ImGui::Checkbox("Override##5",&img_override); ImGui::SameLine(150);ImGui::Text("IMG Program Path (PNG,JPG,GIF)");
            if(img_override) ImGui::InputText("##imgprogrampath",img_program_path,MAX_PATH);
			ImGui::Checkbox("Override##6",&misc_override); ImGui::SameLine(150);ImGui::Text("Misc Program Path");
            if(misc_override) ImGui::InputText("##miscprogrampath",misc_program_path,MAX_PATH);
            ImGui::PopItemWidth();

            ImGui::Separator();
            if(ImGui::Button("Close",ImVec2(settings_init_width,25)))
            { 
                show_settings_window = !show_settings_window;
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }

        // Rendering
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, NULL);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, (float*)&clear_color);
        ImGui::Render();

        g_pSwapChain->Present(1, 0); // Present with vsync

		Sleep(1);

    }

    store_config();
    store_pass();

	t1.join();

    ImGui_ImplDX11_Shutdown();
    CleanupDeviceD3D();
    UnregisterClass(_T("Notes Attic"), wc.hInstance);

    return 0;
}

static void draw_filters_email_list(int x)
{
    // get window size
    ImGui::SetNextWindowPos(ImVec2(x, 22), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(emails_window_width, window_height - 22), ImGuiCond_Always);

    char email_title[32] = {0};
    sprintf(email_title,"Emails [%d/%d]",num_emails_filtered,num_emails);

    ImGui::Begin(email_title, 0,window_flags);

    int list_height_sub = 116;

    if (ImGui::CollapsingHeader("Filters",ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::PushItemWidth(-70);
        ImGui::InputText("To",filter_to,FIELD_VAL_MAX);
        ImGui::InputText("From",filter_from,FIELD_VAL_MAX);
        ImGui::InputText("Date", filter_date, FIELD_VAL_MAX);
        ImGui::InputText("Subject",filter_subject,FIELD_VAL_MAX);
        ImGui::InputText("Body",filter_body,FIELD_VAL_MAX);
        ImGui::PopItemWidth();
        ImGui::Checkbox("Only Attachments",&only_attachments); ImGui::SameLine();

        ImGui::PushAllowKeyboardFocus(false);
        if(ImGui::Button("Apply Filters"))
        {
            apply_filters();
            apply_sorting();
        }

        ImGui::SameLine();
        if(ImGui::Button("Clear Filters"))
        {
            memset(filter_to,0,FIELD_VAL_MAX);
            memset(filter_cc,0,FIELD_VAL_MAX);
            memset(filter_from,0,FIELD_VAL_MAX);
            memset(filter_forwarded,0,FIELD_VAL_MAX);
            memset(filter_date,0,FIELD_VAL_MAX);
            memset(filter_subject,0,FIELD_VAL_MAX);
            memset(filter_body,0,FIELD_VAL_MAX);
            only_attachments = false;

            apply_filters();
            apply_sorting();
        }
        list_height_sub += 156;
        ImGui::PopAllowKeyboardFocus();
    }

    ImGui::Spacing();

    ImGui::PushItemWidth(60);
    if(ImGui::Combo("Dir", &sort_dir, "Asc\0Desc"))
        apply_sorting();
    ImGui::PopItemWidth();
    ImGui::SameLine();

    ImGui::PushItemWidth(emails_window_width-140 - 60);
    if(ImGui::Combo("Sort Field", &sort_field, "Date\0To\0From\0Subject\0CC\0Forwarded"))
        apply_sorting();
    ImGui::PopItemWidth();

    ImGui::TextColored(ImVec4(0.4f, 0.4f,0.4f,1.0f),"%d/%d", selected_email_index + 1, num_emails_filtered); ImGui::SameLine(100);
    ImGui::TextColored(ImVec4(0.4f, 0.4f, 0.4f, 1.0f), "[%3.0f%%]", 100.0f*(selected_email_index + 1) / (float)num_emails_filtered);
    if(num_emails_filtered != num_emails)
    {
        ImGui::SameLine(200);
        ImGui::TextColored(ImVec4(0.4f, 0.6f, 0.6f, 1.0f), "(filters on)");
    }
	
    ImGui::BeginChild(ImGui::GetID((void*)(intptr_t)0), ImVec2(ImGui::GetWindowWidth() - 28, ImGui::GetWindowHeight() -list_height_sub), true);

	ImGuiIO& io = ImGui::GetIO();
    if (!ImGui::IsAnyItemActive())
    {
		if (io.WantCaptureKeyboard)
		{
			if (ImGui::IsKeyPressed(VK_RETURN))
			{
				apply_filters();
				apply_sorting();
			}
		}

        set_scroll_here = false;
        if (ImGui::IsKeyPressed(VK_UP))
        {
            selected_email_index--;
            set_scroll_here = true;

            if (selected_email_index < 0)
                selected_email_index = 0;
        }
        else if (ImGui::IsKeyPressed(VK_DOWN))
        {
            selected_email_index++;
            set_scroll_here = true;

            if (selected_email_index >= num_emails_filtered)
                selected_email_index = num_emails_filtered -1;
        }
        else if (ImGui::IsKeyPressed(VK_PRIOR))
        {
            selected_email_index -= 10;
            set_scroll_here = true;

            if (selected_email_index < 0)
                selected_email_index = 0;
        }
        else if (ImGui::IsKeyPressed(VK_NEXT))
        {
            selected_email_index += 10;
            set_scroll_here = true;

            if (selected_email_index >= num_emails_filtered)
                selected_email_index = num_emails_filtered -1;
        }
        else if (ImGui::IsKeyPressed(VK_HOME))
        {
            selected_email_index = 0;
            set_scroll_here = true;
        }
        else if (ImGui::IsKeyPressed(VK_END))
        {
            selected_email_index = num_emails_filtered -1;
            set_scroll_here = true;
        }

        if (set_scroll_here)
        {
            ImGui::SetScrollFromPosY(ImGui::GetCursorStartPos().y + (selected_email_index*54.0f));
        }
    }

    //     ImGuiListClipper clipper(Items.Size);
    //     while (clipper.Step())
    //         for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)

    ImGuiListClipper clipper(num_emails_filtered);
    while (clipper.Step())
    {
        for(int i = clipper.DisplayStart; i < clipper.DisplayEnd;++i)
        {
            char id[FIELD_VAL_MAX+10] = {0};
            NotesEmail e = emails[filtered_indicies[sorted_indicies[i]]];

            sprintf(id, "%s\n%s\n%s##%d",e.from_formatted, e.date, e.subject,i);

            if (e.attachment_count > 0)
                ImGui::TextColored(ImVec4(0.5f,0.5f,0.2f,1.0f),"@");
            else
                ImGui::Text(" ");

            ImGui::SameLine(25);
            
            //if(set_scroll_here && selected_email_index == i)
            //	ImGui::SetScrollHere();

            if (ImGui::Selectable(id, (selected_email_index == i), 0, ImVec2(ImGui::GetWindowWidth(), 50)))
            {
                selected_email_index = i;
            }
            char context_id[100] = {0};
            sprintf(context_id,"email_context##%d",i);

            if (ImGui::BeginPopupContextItem(context_id))
            {

                if (ImGui::Selectable("Open Email File", false,0))
                {
                    char command[260] = {0};

                    strncpy(command, "\"",2);strcat(command, txt_program_path);strcat(command, "\" ");
                    strcat(command, "\"");strcat(command, e.file_path);strcat(command, "\"");
                    start_process(command);
                }
                if (ImGui::Selectable("Copy Email File Path", false,0))
                {
                    ImGui::SetClipboardText(e.file_path);
                }

                ImGui::Separator();
                if (ImGui::Button("Delete",ImVec2(140,22)))
                    ImGui::OpenPopup("Delete Email");
                if (ImGui::BeginPopupModal("Delete Email", NULL, ImGuiWindowFlags_AlwaysAutoResize))
                {
                    ImGui::Text("This will permanently remove this email!\n%s\n",id);
                    ImGui::Separator();

                    if (ImGui::Button("Delete it!", ImVec2(120,0)))
                    {
                        if (!remove(e.file_path))
                            printf("failed to remove file: %s", e.file_path);

                        delete_email(filtered_indicies[sorted_indicies[i]]);
                        store_cache();
                        apply_filters();
                        apply_sorting();

                        ImGui::CloseCurrentPopup(); 
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Cancel", ImVec2(120,0)))
                    {
                        ImGui::CloseCurrentPopup(); 
                    }
                    ImGui::EndPopup();
                }

                ImGui::EndPopup();
            }
        }
    }

    ImGui::EndChild();

    ImGui::End();

}

static void draw_header_body_attachments(int x)
{
    ImGui::SetNextWindowPos(ImVec2(x, 22), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(window_width - emails_window_width, 154), ImGuiCond_Always);

    ImGui::Begin("Header", 0,window_flags);
    ImGui::PushTextWrapPos(window_width - emails_window_width - 80);
    ImGui::TextColored(header_color, "To:"); ImGui::SameLine(80); ImGui::Text("%s", curr_email.to);
    if (ImGui::BeginPopupContextItem("to_context"))
    {
        if (ImGui::Selectable("Copy - To", false,0))
            if(curr_email.to != NULL) ImGui::SetClipboardText(curr_email.to);
        ImGui::EndPopup();
    }
    ImGui::TextColored(header_color, "CC:"); ImGui::SameLine(80); ImGui::Text("%s", curr_email.cc);
    if (ImGui::BeginPopupContextItem("cc_context"))
    {
        if (ImGui::Selectable("Copy - CC", false,0))
			if (curr_email.cc != NULL) ImGui::SetClipboardText(curr_email.cc);
        ImGui::EndPopup();
    }
    ImGui::TextColored(header_color, "From:"); ImGui::SameLine(80); ImGui::Text("%s", curr_email.from);
    if (ImGui::BeginPopupContextItem("from_context"))
    {
        if (ImGui::Selectable("Copy - From", false,0))
			if (curr_email.from != NULL) ImGui::SetClipboardText(curr_email.from);
        ImGui::EndPopup();
    }
    ImGui::TextColored(header_color, "Fwd From:"); ImGui::SameLine(80); ImGui::Text("%s", curr_email.forwarded);
    if (ImGui::BeginPopupContextItem("fwdfrom_context"))
    {
        if (ImGui::Selectable("Copy - Fwd From", false,0))
			if (curr_email.forwarded != NULL) ImGui::SetClipboardText(curr_email.forwarded);
        ImGui::EndPopup();
    }
    ImGui::TextColored(header_color, "Date:"); ImGui::SameLine(80); ImGui::Text("%s", curr_email.date);
    if (ImGui::BeginPopupContextItem("date_context"))
    {
        if (ImGui::Selectable("Copy - Date", false,0))
			if (curr_email.date != NULL)ImGui::SetClipboardText(curr_email.date);
        ImGui::EndPopup();
    }
    ImGui::TextColored(header_color, "Subject:"); ImGui::SameLine(80); ImGui::Text("%s", curr_email.subject);
    if (ImGui::BeginPopupContextItem("subject_context"))
    {
        if (ImGui::Selectable("Copy - Subject", false,0))
			if (curr_email.subject != NULL)ImGui::SetClipboardText(curr_email.subject);
        ImGui::EndPopup();
    }
    ImGui::PopTextWrapPos();
    ImGui::End();

    float body_width = window_width - emails_window_width;

    ImGui::SetNextWindowPos(ImVec2(x, 176), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(body_width, window_height - 176 - 140), ImGuiCond_Always);
    ImGui::Begin("Body", 0,window_flags);

    if (curr_email.body == NULL)
        ImGui::InputTextMultiline("body", "", FIELD_BODY_MAX, ImVec2(ImGui::GetWindowWidth()-14, ImGui::GetWindowHeight() - 40),ImGuiInputTextFlags_ReadOnly);
    else
        ImGui::InputTextMultiline("body", curr_email.body, FIELD_BODY_MAX, ImVec2(ImGui::GetWindowWidth() - 14, ImGui::GetWindowHeight() - 40), ImGuiInputTextFlags_ReadOnly);
    if (ImGui::BeginPopupContextItem("body"))
    {
        if (ImGui::Selectable("Copy - Body", false,0))
            ImGui::SetClipboardText(curr_email.body);
        ImGui::EndPopup();
    }

    ImGui::End();

    float attachments_width = window_width - emails_window_width;

    ImGui::SetNextWindowPos(ImVec2(x,window_height - 140), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(attachments_width,140), ImGuiCond_Always);
    ImGui::Begin("Attachments", 0,window_flags);
    ImGui::Text("Attachment Count: %d", curr_email.attachment_count);

    ImGui::BeginChild(ImGui::GetID((void*)(intptr_t)1), ImVec2(attachments_width - 20,80), true,ImGuiWindowFlags_HorizontalScrollbar);
    for (int i = 0; i < curr_email.attachment_count; ++i)
    {
        char id[FIELD_VAL_MAX+10] = {0};
        sprintf(id, "%s##%d",curr_email.attachments_ext[i], i);

        if (ImGui::Button(id, ImVec2(50, 50)))
        {
            char* ext = curr_email.attachments_ext[i];
            str_tolower(ext);
            char command[260] = {0};

            char full_attachment_path[260] = {0};

            if(str_startswith(curr_email.attachments[i],"\\"))
            {
                strncpy(full_attachment_path, root_dir,strlen(root_dir));
                strcat(full_attachment_path, curr_email.attachments[i]);
            }
            else
                strncpy(full_attachment_path,curr_email.attachments[i],strlen(curr_email.attachments[i]));

            if (strcmp(ext, "xls") == 0 || strcmp(ext, "xlsx") == 0 || strcmp(ext, "xlsm") == 0)
            {
                if(xls_override)
                {
                    strncpy(command, "\"",2);strcat(command, xls_program_path);strcat(command, "\" ");
                    strcat(command, "\"");strcat(command, full_attachment_path);strcat(command, "\"");
                    start_process(command);
                }
                else
                {
                    start_default_process(full_attachment_path);
                }
            }
            else if (strcmp(ext, "doc") == 0 || strcmp(ext, "docx") == 0)
            {
                if(doc_override)
                {
                    strncpy(command, "\"",2);strcat(command, doc_program_path);strcat(command, "\" ");
                    strcat(command, "\"");strcat(command, full_attachment_path);strcat(command, "\"");
                    start_process(command);
                }
                else
                {
                    start_default_process(full_attachment_path);
                }
            }
            else if (strcmp(ext, "csv") == 0)
            {
                if(csv_override)
                {
                    strncpy(command, "\"",2);strcat(command, csv_program_path);strcat(command, "\" ");
                    strcat(command, "\"");strcat(command, full_attachment_path);strcat(command, "\"");
                    start_process(command);
                }
                else
                {
                    start_default_process(full_attachment_path);
                }
            }
            else if (strcmp(ext, "txt")== 0)
            {
                if(txt_override)
                {
                    strncpy(command, "\"",2);strcat(command, txt_program_path);strcat(command, "\" ");
                    strcat(command, "\"");strcat(command, full_attachment_path);strcat(command, "\"");
                    start_process(command);
                }
                else
                {
                    start_default_process(full_attachment_path);
                }
            }
            else if (strcmp(ext, "pdf") == 0 || strcmp(ext, "pdfx") == 0)
            {
                if(pdf_override)
                {
                    strncpy(command, "\"",2);strcat(command, pdf_program_path);strcat(command, "\" ");
                    strcat(command, "\"");strcat(command, full_attachment_path);strcat(command, "\"");
                    start_process(command);
                }
                else
                {
                    start_default_process(full_attachment_path);
                }
            }
            else if (strcmp(ext, "jpg") == 0 || strcmp(ext, "png") == 0 || strcmp(ext, "gif") == 0)
            {
                if(img_override)
                {
                    strcat(command, img_program_path);
                    strcat(command, " ");
                    strcat(command, full_attachment_path);
                    start_process(command);
                }
                else
                {
                    start_default_process(full_attachment_path);
                }
            }
            else
            {
                if(misc_override)
                {
                    strncpy(command, "\"",2);strcat(command, misc_program_path);strcat(command, "\" ");
                    strcat(command, "\"");strcat(command, full_attachment_path);strcat(command, "\"");
                    start_process(command);
                }
                else
                {
                    start_default_process(full_attachment_path);
                }
            }
        }
        char context_id[100] = {0};
        sprintf(context_id,"context##%d",i);

        if (ImGui::BeginPopupContextItem(context_id))
        {
            if (ImGui::Selectable("Copy Attachment Path", false,0))
            {
                char full_attachment_path[260] = {0};

                if(str_startswith(curr_email.attachments[i],"\\"))
                {
                    strncpy(full_attachment_path, root_dir,strlen(root_dir));
                    strcat(full_attachment_path, curr_email.attachments[i]);
                }
                else
                    strncpy(full_attachment_path,curr_email.attachments[i],strlen(curr_email.attachments[i]));

                ImGui::SetClipboardText(full_attachment_path);
            }
            ImGui::EndPopup();
        }

        if (ImGui::IsItemHovered())
        {
            ImGui::BeginTooltip();
            ImGui::Text("%s",curr_email.attachments[i]);
            ImGui::EndTooltip();
        }
        ImGui::SameLine();

    }
    ImGui::EndChild();
    ImGui::End();

}

static void start_default_process(const char* path)
{
    char command[260] = {0};

    strncpy(command,"cmd /c \"start /B \"\" \"",21);
    strcat(command,path);
    strcat(command,"\"\"");
    start_process(command);
}

static bool directory_exists(char* dir)
{

	DWORD dwAttrib = GetFileAttributesA((LPCSTR)dir);

	return (dwAttrib != INVALID_FILE_ATTRIBUTES &&
		(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

static bool file_exists(char* file)
{
	WIN32_FIND_DATAA FindFileData;
	HANDLE handle = FindFirstFileA(file, &FindFileData);
	int found = handle != INVALID_HANDLE_VALUE;
	if (found)
	{
		FindClose(handle);
	}
	return found;
}

static char* get_app_full_file_path(char* relative_file_path)
{
        char full_path[MAX_PATH] = {0};

        char* app_dir = get_app_directory();
        strncpy(full_path,app_dir,strlen(app_dir));
        strncat(full_path,relative_file_path, strlen(relative_file_path));

		return full_path;
}

static char* get_app_directory()
{
    HMODULE hModule = GetModuleHandleA(NULL);
    char path[MAX_PATH] = {0};
    GetModuleFileNameA(hModule, path, MAX_PATH);

    int path_length = strlen(path);
    int binary_length = 0;
    for(int i = path_length - 1; i >= 0; --i)
    {
        if(path[i] == '\\' || path[i] == '/')
            break;

        ++binary_length;
    }

    char just_dir[MAX_PATH] = {0};
    strncpy(just_dir,path,path_length - binary_length);

    return just_dir;
}

static char* dt_convert_to_sortable_format(const char* date)
{
    // convert
    // MM/dd/yyyy hh:mm tt
    // to
    // yyyyMMddHHmm

	if (date == NULL)
		return "";

    char* p = (char*)date;
    int   d_type = 0;

    char   MM[2] = {0};
    char   dd[2] = {0};
    char yyyy[4] = {0};
    char   hh[2] = {0};
    char   mm[2] = {0};
    char   tt[2] = {0};

    char c;
    int index = 0;
	
    while(*p)
    {
        c = *p++;

        if(c == '/' || c == ':' || c == ' ')
        {
            d_type++;
            index = 0;
        }
        else
        {
            switch(d_type)
            {
                case 0:   MM[index++] = c; break; // MM
                case 1:   dd[index++] = c; break; // dd
                case 2: yyyy[index++] = c; break; // yyyy
                case 3:   hh[index++] = c; break; // hh
                case 4:   mm[index++] = c; break; // mm
                case 5:   tt[index++] = c; break; // tt
            }
        }
    }

    char* sortable_date = (char*)calloc(12 + 1,sizeof(char));

    if(tt[0] == 'P' && !(hh[0] == '1' && hh[1] == '2'))
    {
        // add 12 to the hour component if PM (except 12 PM)
        // to make it military time

        int hour = atoi(hh);
        hour += 12;
		
		char HH[3] = { 0 };
        sprintf(HH, "%d",hour);
		hh[0] = HH[0];
		hh[1] = HH[1];
    }

	strncpy(sortable_date, yyyy, 4);
	strncat(sortable_date, MM, 2);
	strncat(sortable_date, dd, 2);
	//strncat(sortable_date, tt, 2);
	strncat(sortable_date, hh, 2);
	strncat(sortable_date, mm, 2);

    return sortable_date;
}

static void get_emails_direct(const char* directory_path,FILE* cache_fp,int* temp_num_emails)
{
    DIR *dir;
    struct dirent *dp;

    dir = opendir(directory_path);

    if(dir == NULL)
        return;

    for(;;)
    {
        dp = readdir(dir);

        if(dp == NULL)
            break;

		if (strcmp(dp->d_name,".") == 0 || strcmp(dp->d_name,"..") == 0)
			continue;

        char full_path[MAX_PATH] = {0};
        strncpy(full_path,directory_path,strlen(directory_path));
        strcat(full_path,"\\");
        strcat(full_path,dp->d_name);

        if(str_startswith(dp->d_name,"email_"))
        {
            fprintf(cache_fp, "%d;%s", strlen(full_path), full_path); // file_path

            // try to open file and read in contents
            FILE* fp = fopen(full_path, "r");

            if (fp == NULL)
                continue;

            char key[FIELD_KEY_MAX] = { 0 };
            char val[FIELD_VAL_MAX] = { 0 };

            int key_count = 0;
            int val_count = 0;
            char c = '0';

            int num_attachments = 0;

            bool on_key = true;
            bool val_start = false;
			int  attachment_count = 0;

            char*  body = NULL;
            char*  to = NULL;
            char*  to_formatted = NULL;
            char*  cc = NULL;
            char*  from = NULL;
            char*  from_formatted = NULL;
            char*  forwarded = NULL;
            char*  date = NULL;
            char*  date_formatted = NULL;
            char*  subject = NULL;
            char** attachments = NULL;
            char** attachments_ext = NULL;

            while (c != EOF)
            {
                if (val_count >= FIELD_VAL_MAX -1)
                    val_count = FIELD_VAL_MAX -1;

                if (key_count >= FIELD_KEY_MAX -1)
                    key_count = FIELD_KEY_MAX -1;

                c = (char)fgetc(fp);

                if (val_start)
                {
                    // read past initial whitespace
                    while (c != EOF && (c == '\t' || c == ' '))
                        c = (char)fgetc(fp);

                    val_start = false;
                }

                switch (c)
                {
                case ':':
                {
                    if (on_key)
                    {
                        val_count = 0;
                        memset(val, 0, FIELD_VAL_MAX);
                        on_key = false;
                        val_start = true;
                    }
                    else
                    {
                        val[val_count++] = c;
                    }
                } break;

                case '\r':
                case '\n':
                {
                    if (strcmp(key, "Body") == 0)
                    {
                        do
                        {
                            c = (char)fgetc(fp);
                        } while (c != EOF && (c == '\n' || c == '\t' || c == '\r' || c == ' '));

                        //char body[1000000] = { 0 };
                        char* temp_body = (char*)calloc(1000000, sizeof(char));

                        temp_body[0] = c;
                        int body_length = 1;

                        for (;;)
                        {
                            c = (char)fgetc(fp);

                            if (c == EOF)
                                break;

                            temp_body[body_length++] = c;

                            if(body_length == 1000000)
                                break;
                        }

                        // copy body to email body
                        body = (char*)calloc(body_length+1,sizeof(char));
                        strncpy(body, temp_body,body_length);

                        free(temp_body);
                    }
                    else
                    {
                        int val_length = strlen(val);

                        if (strcmp(key, "To") == 0)
                        {
                            to = (char*)calloc(val_length+1,sizeof(char));
                            strncpy(to,val,val_length);
                        }
                        else if (strcmp(key, "CC") == 0)
                        {
                            cc = (char*)calloc(val_length+1,sizeof(char));
                            strncpy(cc, val,val_length);
                        }
                        else if (strcmp(key, "From") == 0)
                        {
                            from = (char*)calloc(val_length+1,sizeof(char));
                            strncpy(from, val,val_length);
                        }
                        else if (strcmp(key, "Forwarded From") == 0)
                        {
                            forwarded = (char*)calloc(val_length+1,sizeof(char));
                            strncpy(forwarded, val,val_length);
                        }
                        else if (strcmp(key, "Date") == 0)
                        {
                            date = (char*)calloc(val_length+1,sizeof(char));
                            strncpy(date, val,val_length);
                        }
                        else if (strcmp(key, "Subject") == 0)
                        {
                            subject = (char*)calloc(val_length+1,sizeof(char));
                            strncpy(subject, val,val_length);
                        }
                        else if (strcmp(key, "Attachment_Count") == 0)
                        {
                            attachment_count = atoi(val);
                            attachments     = (char**)calloc(attachment_count, sizeof(char*));
                            attachments_ext = (char**)calloc(attachment_count, sizeof(char*));
                        }
                        else if (str_startswith(key,"Attachment_"))
                        {
                            int len = strlen(val);
                            attachments[num_attachments] = (char*)calloc(len+1,sizeof(char));
                            strncpy(attachments[num_attachments],val,len);
                            attachments_ext[num_attachments] = str_getext(val);

                            ++num_attachments;
                        }

                        key_count = 0;
                        on_key = true;
                        memset(key, 0, FIELD_KEY_MAX);

                    }
                } break;
                default:
                {
                    if (on_key)
                        key[key_count++] = c;
                    else
                        val[val_count++] = c;
                } break;
                }

            }

            fclose(fp);
            
            // post-process fields
            // store sortable date
            date_formatted = dt_convert_to_sortable_format(date);

            // store formatted to
            int to_length = strlen(to);
            char* to_temp = (char*)calloc(to_length+1, sizeof(char));

            int latest_length = str_replace(to,"CN=","",to_temp);

            to_formatted = (char*)calloc(latest_length+1, sizeof(char));
            str_replace(to_temp,"/O=ONLI@ONLI","",to_formatted);

            free(to_temp);

            // store formatted from
            int from_length = strlen(from);
            char* from_temp = (char*)calloc(from_length + 1, sizeof(char));

            int latest_length_from = str_replace(from, "CN=", "", from_temp);

            from_formatted = (char*)calloc(latest_length_from + 1, sizeof(char));
            str_replace(from_temp, "/O=ONLI", "", from_formatted);

            free(from_temp);

            // write other email fields to cache
            fprintf(cache_fp, "%d;%s", strlen(to), to);
            fprintf(cache_fp, "%d;%s", strlen(to_formatted), to_formatted);
            fprintf(cache_fp, "%d;%s", strlen(cc), cc);
            fprintf(cache_fp, "%d;%s", strlen(from), from);
            fprintf(cache_fp, "%d;%s", strlen(from_formatted), from_formatted);
            fprintf(cache_fp, "%d;%s", strlen(forwarded), forwarded);
            fprintf(cache_fp, "%d;%s", strlen(date), date);
            fprintf(cache_fp, "%d;%s", strlen(date_formatted), date_formatted);
            fprintf(cache_fp, "%d;%s", strlen(subject), subject);
            fprintf(cache_fp, "%d;", attachment_count);

            for(int j = 0; j < attachment_count; ++j)
            {
                fprintf(cache_fp, "%d;%s", strlen(attachments[j]), attachments[j]);
                fprintf(cache_fp, "%d;%s", strlen(attachments_ext[j]), attachments_ext[j]);
            }

            fprintf(cache_fp, "%d;%s", strlen(body), body);

			if (to != NULL) free(to);
			if (to_formatted != NULL) free(to_formatted);
			if (cc != NULL) free(cc);
			if (from != NULL) free(from);
			if (from_formatted != NULL) free(from_formatted);
			if (forwarded != NULL) free(forwarded);
			if (date != NULL) free(date);
			if (date_formatted != NULL) free(date_formatted);
			if (subject != NULL) free(subject);
			if (body != NULL) free(body);

            for(int j = 0; j < attachment_count; ++j)
            {
                if (attachments[j] != NULL) free(attachments[j]);
                if (attachments_ext[j] != NULL) free(attachments_ext[j]);
            }

			if (attachments != NULL) free(attachments);
			if (attachments_ext != NULL) free(attachments_ext);

            attachment_count = 0;

			++(*temp_num_emails);
        }

        get_emails_direct(full_path,cache_fp,temp_num_emails);

        Sleep(1);
    }

    closedir(dir);
}

static void write_direct_to_cache(const char* directory_path)
{
    FILE *cache_fp = fopen(cache_file_path_temp,"wb");

    if(cache_fp == NULL)
        return;

    // write space at beginning for number of emails
    fputc(' ',cache_fp);
    fputc(' ',cache_fp);
    fputc(' ',cache_fp);
    fputc(' ',cache_fp);
    fputc(' ',cache_fp);
    fputc(' ',cache_fp);
    fputc(' ',cache_fp);
    fputc(' ',cache_fp);
    fputc(' ',cache_fp);
    fputc(' ',cache_fp);
    fputc(';' ,cache_fp);

	int temp_num_emails = 0;
    get_emails_direct(root_dir,cache_fp,&temp_num_emails);

    // fill in number of emails in cache
    fseek(cache_fp,0,SEEK_SET);
    fprintf(cache_fp,"%d",temp_num_emails);

    fclose(cache_fp);

    // rename temp cache to real cache
    if(!MoveFileA(cache_file_path,cache_file_path_del))  return;
    if(!MoveFileA(cache_file_path_temp,cache_file_path)) return;
    DeleteFileA(cache_file_path_del);

    // set num_emails to new num_emails
    num_emails = temp_num_emails;
     
    // mark update cache flag for main thread to refresh
    cache_is_updated = true;
}

static void store_cache()
{
    FILE *fp = fopen(cache_file_path,"wb");

    if(fp == NULL)
        return;

	fprintf(fp, "%d;", num_emails);

    for(int i = 0; i < num_emails; ++i)
    {
		fprintf(fp, "%d;%s", strlen(emails[i].file_path), emails[i].file_path);
		fprintf(fp, "%d;%s", strlen(emails[i].to), emails[i].to);
		fprintf(fp, "%d;%s", strlen(emails[i].to_formatted), emails[i].to_formatted);
		fprintf(fp, "%d;%s", strlen(emails[i].cc), emails[i].cc);
		fprintf(fp, "%d;%s", strlen(emails[i].from), emails[i].from);
		fprintf(fp, "%d;%s", strlen(emails[i].from_formatted), emails[i].from_formatted);
		fprintf(fp, "%d;%s", strlen(emails[i].forwarded), emails[i].forwarded);
		fprintf(fp, "%d;%s", strlen(emails[i].date), emails[i].date);
		fprintf(fp, "%d;%s", strlen(emails[i].date_formatted), emails[i].date_formatted);
		fprintf(fp, "%d;%s", strlen(emails[i].subject), emails[i].subject);
		fprintf(fp, "%d;", emails[i].attachment_count);

		for(int j = 0; j < emails[i].attachment_count; ++j)
        {
			fprintf(fp, "%d;%s", strlen(emails[i].attachments[j]), emails[i].attachments[j]);
			fprintf(fp, "%d;%s", strlen(emails[i].attachments_ext[j]), emails[i].attachments_ext[j]);
        }
		fprintf(fp, "%d;%s", strlen(emails[i].body), emails[i].body);
    }

    fclose(fp);
}

static bool load_cache()
{
    FILE *fp = fopen(cache_file_path,"rb");

    if(fp == NULL)
        return false;

	fscanf(fp, "%d ;", &num_emails);

	filtered_indicies = (int*)malloc(num_emails*sizeof(int));
	sorted_indicies = (int*)malloc(num_emails*sizeof(int));

	for (int i = 0; i < num_emails; ++i)
	{
        int str_len;

		// file_path
		fscanf(fp, "%d;", &str_len);
		emails[i].file_path = (char*)calloc(str_len+1, sizeof(char));

		for (int j = 0; j < str_len; ++j)
			emails[i].file_path[j] = (char)fgetc(fp);

		// to
		fscanf(fp, "%d;", &str_len);
		emails[i].to = (char*)calloc(str_len+1, sizeof(char));

		for (int j = 0; j < str_len; ++j)
			emails[i].to[j] = (char)fgetc(fp);
        
		// to_formatted
		fscanf(fp, "%d;", &str_len);
		emails[i].to_formatted = (char*)calloc(str_len+1, sizeof(char));

		for (int j = 0; j < str_len; ++j)
			emails[i].to_formatted[j] = (char)fgetc(fp);

		// cc
		fscanf(fp, "%d;", &str_len);
		emails[i].cc = (char*)calloc(str_len+1, sizeof(char));

		for (int j = 0; j < str_len; ++j)
			emails[i].cc[j] = (char)fgetc(fp);

		// from
		fscanf(fp, "%d;", &str_len);
		emails[i].from = (char*)calloc(str_len+1, sizeof(char));

		for (int j = 0; j < str_len; ++j)
			emails[i].from[j] = (char)fgetc(fp);

		// from formatted
		fscanf(fp, "%d;", &str_len);
		emails[i].from_formatted = (char*)calloc(str_len+1, sizeof(char));

		for (int j = 0; j < str_len; ++j)
			emails[i].from_formatted[j] = (char)fgetc(fp);

		// forwarded
		fscanf(fp, "%d;", &str_len);
		emails[i].forwarded = (char*)calloc(str_len+1, sizeof(char));

		for (int j = 0; j < str_len; ++j)
			emails[i].forwarded[j] = (char)fgetc(fp);

		// date
		fscanf(fp, "%d;", &str_len);
		emails[i].date = (char*)calloc(str_len+1, sizeof(char));

		for (int j = 0; j < str_len; ++j)
			emails[i].date[j] = (char)fgetc(fp);

		// date formatted
		fscanf(fp, "%d;", &str_len);
		emails[i].date_formatted = (char*)calloc(str_len+1, sizeof(char));

		for (int j = 0; j < str_len; ++j)
			emails[i].date_formatted[j] = (char)fgetc(fp);

		// subject
		fscanf(fp, "%d;", &str_len);
		emails[i].subject = (char*)calloc(str_len+1, sizeof(char));

		for (int j = 0; j < str_len; ++j)
			emails[i].subject[j] = (char)fgetc(fp);

		// attachment_count
		fscanf(fp, "%d;", &emails[i].attachment_count);

		// attachments
		emails[i].attachments     = (char**)calloc(emails[i].attachment_count, sizeof(char*));
		emails[i].attachments_ext = (char**)calloc(emails[i].attachment_count, sizeof(char*));

		for (int k = 0; k < emails[i].attachment_count;++k)
		{
			//path
			fscanf(fp, "%d;", &str_len);
			emails[i].attachments[k] = (char*)calloc(str_len+1, sizeof(char));

			for (int j = 0; j < str_len; ++j)
				emails[i].attachments[k][j] = (char)fgetc(fp);

			// ext
			fscanf(fp, "%d;", &str_len);
			emails[i].attachments_ext[k] = (char*)calloc(str_len+1, sizeof(char));

			for (int j = 0; j < str_len; ++j)
				emails[i].attachments_ext[k][j] = (char)fgetc(fp);
		}
        
		// body
		fscanf(fp, "%d;", &str_len);
		emails[i].body = (char*)calloc(str_len+1, sizeof(char));

		for (int j = 0; j < str_len; ++j)
			emails[i].body[j] = (char)fgetc(fp);
	}

    fclose(fp);
	return true;
}

static void post_process_email_fields()
{
    for(int i = 0; i < num_emails;++i)
    {
        // store sortable date
        emails[i].date_formatted = dt_convert_to_sortable_format(emails[i].date);

        // store formatted to
		int to_length = strlen(emails[i].to);
        char* to_temp = (char*)calloc(to_length+1, sizeof(char));

        int latest_length = str_replace(emails[i].to,"CN=","",to_temp);

		emails[i].to_formatted = (char*)calloc(latest_length+1, sizeof(char));
        str_replace(to_temp,"/O=ONLI@ONLI","",emails[i].to_formatted);

        free(to_temp);

		// store formatted from
		int from_length = strlen(emails[i].from);
		char* from_temp = (char*)calloc(from_length + 1, sizeof(char));

		int latest_length_from = str_replace(emails[i].from, "CN=", "", from_temp);

		emails[i].from_formatted = (char*)calloc(latest_length_from + 1, sizeof(char));
		str_replace(from_temp, "/O=ONLI", "", emails[i].from_formatted);

		free(from_temp);

    }
}

static void load_emails()
{
    clear_email_memory();
	num_emails = 0;
    get_emails(root_dir);

    if(num_emails > MAX_EMAILS)
        num_emails = MAX_EMAILS;

	filtered_indicies = (int*)malloc(num_emails*sizeof(int));
	sorted_indicies   = (int*)malloc(num_emails*sizeof(int));

    for(int i = 0; i < num_emails; ++i)
        parse_email(emails[i].file_path,i);
    
	post_process_email_fields();
    store_cache();
}

static void reset_and_load_emails()
{
    load_emails();
    apply_filters();
    apply_sorting();
}

static void apply_sorting()
{
	SortItem* items = (SortItem*)malloc(num_emails_filtered * sizeof(SortItem));

    // prepare sort items
    for (int i = 0; i < num_emails_filtered; ++i)
    {
        items[i].index = i;

        NotesEmail e = emails[filtered_indicies[i]];

        switch(sort_field)
        {
            case TO:        items[i].value = e.to_formatted; break;
            case CC:        items[i].value = e.cc; break;
            case FROM:      items[i].value = e.from_formatted; break;
            case FORWARDED: items[i].value = e.forwarded; break;
            case NOTES_DATE:      items[i].value = e.date_formatted; break;
            case SUBJECT:   items[i].value = e.subject; break;
        }
    }

    // sort
    quicksort(items,0,num_emails_filtered-1);

    // copy sort indicies to array
    if(sort_dir == 0)
    {
        for(int i = 0; i < num_emails_filtered; ++i)
            sorted_indicies[i] = items[i].index;
    }
    else
    {
        for(int i = 0; i < num_emails_filtered; ++i)
            sorted_indicies[i] = items[num_emails_filtered - i - 1].index;
    }

	free(items);
}

static void apply_filters()
{
	prev_selected_email_index = -1;
    num_emails_filtered = 0;

    char compare[500000] = {0};
    char filter[1000] = {0};

    for(int i = 0; i < num_emails; ++i)
    {
        bool match = true;

        for(int j = 0; j < 7; ++j)
        {
            memset(compare, 0, 500000);
            memset(filter,  0, 1000);

            switch(j)
            {
				case NOTES_DATE: strncpy(filter, filter_date, strlen(filter_date)); strncpy(compare, emails[i].date, strlen(emails[i].date)); break;
                case TO: strncpy(filter,filter_to,strlen(filter_to)); strncpy(compare,emails[i].to_formatted,strlen(emails[i].to)); break;
				case FROM: strncpy(filter, filter_from, strlen(filter_from)); strncpy(compare, emails[i].from_formatted, strlen(emails[i].from)); break;
				case SUBJECT: strncpy(filter, filter_subject, strlen(filter_subject)); strncpy(compare, emails[i].subject, strlen(emails[i].subject)); break;
                case CC: strncpy(filter,filter_cc,strlen(filter_cc)); strncpy(compare,emails[i].cc,strlen(emails[i].cc)); break;
                case FORWARDED: strncpy(filter,filter_forwarded,strlen(filter_forwarded)); strncpy(compare,emails[i].forwarded,strlen(emails[i].forwarded)); break;
                case BODY: strncpy(filter,filter_body,strlen(filter_body)); strncpy(compare,emails[i].body,strlen(emails[i].body)); break;
            }

            if ((!str_contains(compare, filter,false) && strcmp(filter, "") != 0) || (only_attachments && emails[i].attachment_count <= 0))
            {
                match = false;
                break;
            }
        }

		if (match)
        {
            filtered_indicies[num_emails_filtered++] = i;
        }
    }

}

static char* str_getext(const char* file_path)
{
    char* p = (char*)file_path;

    // go to end of c-string
    int path_length = 0;
    while(*p)
    {
        ++path_length;
        ++p;
    }

	// work backwards to find first occurrence of '.'
	int dot_index = -1;
	for (int i = path_length -1; i >= 0; --i)
	{
		if (file_path[i] == '.')
		{
			dot_index = i;
			break;
		}
	}
	
    int ext_length;

	if (dot_index == -1)
		ext_length = 0;
    else
	    ext_length = path_length - dot_index - 1;

	char* return_ext = (char*)calloc(ext_length + 1, sizeof(char));

    if(dot_index == -1)
        return_ext[0] = 0;
    else
    {
        int curr_ext_length = 0;
        for(int i = dot_index + 1; i < path_length; ++i)
            return_ext[curr_ext_length++] = file_path[i];
    }

	return return_ext;
}

static void str_tolower(char *str)
{
    while(*str != '\0')
    {
        if(*str >=65 && *str<=90)
            *str = *str + 32;
        str++;
    }
}

static bool str_startswith(char* base, char* search_pattern)
{
	if (strcmp(search_pattern,"") == 0)
		return false;

    char* b_p = base;
    char* p_p = search_pattern;

    while(*p_p)
    {
        if(*b_p == NULL)
            return false;

        if(*p_p != *b_p)
            return false;

        ++p_p;
        ++b_p;
    }

    return true;
}

static bool str_contains(char* base, char* search_pattern,bool case_sensitive = true)
{

	if (strcmp(search_pattern,"") == 0)
		return false;

    char* b_p = base;
    char* p_p = search_pattern;

    char  b,p,bb,pp;
    
    while(*b_p)
    {
        b = *b_p;
        p = *p_p;

        if(!case_sensitive)
        {
            // turn characters into lower-case if needed
            if(b >= 65 && b <= 90)
                b += 32;

            if(p >= 65 && p <= 90)
                p += 32;
        }

        if(b == p)
        {
            char* bb_p = b_p;
            bool match = true;
            int pattern_length = 0;

            while(*p_p)
            {
                bb = *bb_p;
                pp = *p_p;

                if(!case_sensitive)
                {
                    // turn characters into lower-case if needed
                    if(bb >= 65 && bb <= 90)
                        bb += 32;

                    if(pp >= 65 && pp <= 90)
                        pp += 32;
                }

                if(bb != pp)
                    match = false;
                
                ++p_p;
                ++bb_p;
                ++pattern_length;
            }

            p_p = search_pattern;

            if(match)
            {
                return true;
            }
        }

        ++b_p;
    }

    return false;
}

static int str_replace(char* base, char* search_pattern, char* replacement, char* ret_string)
{
    int  ret_string_i = 0;

    char* b_p = base;
    char* p_p = search_pattern;
    char* r_p = replacement;

    while(*b_p)
    {
        if(*b_p == *p_p)
        {
            char* bb_p = b_p;
			char* pp_p = p_p;

            BOOL match = TRUE;
            int pattern_length = 0;

            while(*pp_p)
            {
				if (*bb_p != *pp_p)
				{
					match = FALSE;
					break;
				}

                ++pp_p;
                ++bb_p;
                ++pattern_length;
            }

            if(match)
            {
                // replace search_pattern with replacement in base
                while(*r_p)
                    ret_string[ret_string_i++] = *r_p++;

                //reset pointers
                b_p += pattern_length;
                p_p = search_pattern;
                r_p = replacement;
            }
        }

        ret_string[ret_string_i++] = *b_p++;
    }

	if (ret_string == NULL)
		return 0;

	ret_string[ret_string_i] = '\0';
	return ret_string_i+1;
}

static void parse_email(const char* file_path,int index)
{
	// try to open file and read in contents
	FILE* fp = fopen(file_path, "r");

	if (!fp == NULL)
	{
		char key[FIELD_KEY_MAX] = { 0 };
		char val[FIELD_VAL_MAX] = { 0 };

		int key_count = 0;
		int val_count = 0;
		char c = '0';

        int num_attachments = 0;

		bool on_key = true;
		bool val_start = false;

		while (c != EOF)
		{
			if (val_count >= FIELD_VAL_MAX -1)
				val_count = FIELD_VAL_MAX -1;

			if (key_count >= FIELD_KEY_MAX -1)
				key_count = FIELD_KEY_MAX -1;

			c = (char)fgetc(fp);

			if (val_start)
			{
				// read past initial whitespace
				while (c != EOF && (c == '\t' || c == ' '))
					c = (char)fgetc(fp);

				val_start = false;
			}

			switch (c)
			{
			case ':':
			{
				if (on_key)
				{
					val_count = 0;
					memset(val, 0, FIELD_VAL_MAX);
					on_key = false;
					val_start = true;
				}
				else
				{
					val[val_count++] = c;
				}
			} break;

			case '\r':
			case '\n':
			{
				if (strcmp(key, "Body") == 0)
				{
					do
					{
						c = (char)fgetc(fp);
					} while (c != EOF && (c == '\n' || c == '\t' || c == '\r' || c == ' '));

					//char body[1000000] = { 0 };
					char* body = (char*)calloc(1000000, sizeof(char));

					body[0] = c;
					int body_length = 1;

					for (;;)
					{
						c = (char)fgetc(fp);

						if (c == EOF)
							break;

						body[body_length++] = c;

                        if(body_length == 1000000)
                            break;
					}

					// copy body to email body
					emails[index].body = (char*)calloc(body_length+1,sizeof(char));
					strncpy(emails[index].body, body,body_length);

                    free(body);
				}
				else
				{
                    int val_length = strlen(val);

					if (strcmp(key, "To") == 0)
                    {
                        emails[index].to = (char*)calloc(val_length+1,sizeof(char));
                        strncpy(emails[index].to,val,val_length);
                    }
					else if (strcmp(key, "CC") == 0)
                    {
                        emails[index].cc = (char*)calloc(val_length+1,sizeof(char));
                        strncpy(emails[index].cc, val,val_length);
                    }
					else if (strcmp(key, "From") == 0)
                    {
                        emails[index].from = (char*)calloc(val_length+1,sizeof(char));
                        strncpy(emails[index].from, val,val_length);
                    }
					else if (strcmp(key, "Forwarded From") == 0)
                    {
                        emails[index].forwarded = (char*)calloc(val_length+1,sizeof(char));
                        strncpy(emails[index].forwarded, val,val_length);
                    }
					else if (strcmp(key, "Date") == 0)
                    {
                        emails[index].date = (char*)calloc(val_length+1,sizeof(char));
                        strncpy(emails[index].date, val,val_length);
                    }
					else if (strcmp(key, "Subject") == 0)
                    {
                        emails[index].subject = (char*)calloc(val_length+1,sizeof(char));
                        strncpy(emails[index].subject, val,val_length);
                    }
					else if (strcmp(key, "Attachment_Count") == 0)
                    {
						emails[index].attachment_count = atoi(val);
						emails[index].attachments     = (char**)calloc(emails[index].attachment_count, sizeof(char*));
						emails[index].attachments_ext = (char**)calloc(emails[index].attachment_count, sizeof(char*));
                    }
                    else if (str_startswith(key,"Attachment_"))
                    {
                        int len = strlen(val);
                        emails[index].attachments[num_attachments] = (char*)calloc(len+1,sizeof(char));
                        strncpy(emails[index].attachments[num_attachments],val,len);
                        emails[index].attachments_ext[num_attachments] = str_getext(val);

                        ++num_attachments;
                    }

					key_count = 0;
					on_key = true;
					memset(key, 0, FIELD_KEY_MAX);

				}
			} break;
			default:
			{
				if (on_key)
					key[key_count++] = c;
				else
					val[val_count++] = c;
			} break;
			}

		}
		fclose(fp);
	}
}

static void store_pass()
{
    FILE *fp = fopen(bin_file_path,"wb");
	
	if (fp == NULL)
		return;

	char encrypted_password[BIN_LENGTH + 1] = { 0 };
	strncpy(encrypted_password, encrypt_password(notes_password), BIN_LENGTH);

	for(int i = 0; i < BIN_LENGTH; ++i)
		fputc(encrypted_password[i],fp);

    fclose(fp);
}

static void load_pass()
{
    FILE *fp = fopen(bin_file_path,"rb");
	
	if (fp == NULL)
		return;

    char encrypted_password[BIN_LENGTH+1] = {0};

    int pass_length = 0;
    char c;
    for(;;)
    {
        c = fgetc(fp);

        if(c == EOF)
            break;

        encrypted_password[pass_length++] = c;
    }

    strncpy(notes_password,decrypt_password(encrypted_password), pass_length);

    fclose(fp);
}

static void store_config()
{
    FILE *fp = fopen(config_file_path,"w");
	
	if (fp == NULL)
		return;

	fprintf(fp,"root_dir: %s\n",root_dir);
    fprintf(fp,"xls_override: %d\n", xls_override);
	fprintf(fp,"xls_program_path: %s\n",xls_program_path);
    fprintf(fp,"doc_override: %d\n", doc_override);
	fprintf(fp,"doc_program_path: %s\n",doc_program_path);
    fprintf(fp,"csv_override: %d\n", csv_override);
	fprintf(fp,"csv_program_path: %s\n",csv_program_path);
    fprintf(fp,"txt_override: %d\n", txt_override);
	fprintf(fp,"txt_program_path: %s\n",txt_program_path);
    fprintf(fp,"pdf_override: %d\n", pdf_override);
	fprintf(fp,"pdf_program_path: %s\n",pdf_program_path);
    fprintf(fp,"img_override: %d\n", img_override);
	fprintf(fp,"img_program_path: %s\n",img_program_path);
    fprintf(fp,"misc_override: %d\n", misc_override);
	fprintf(fp,"misc_program_path: %s\n",misc_program_path);
	fprintf(fp,"export_program_path: %s\n",export_program_path);
    fprintf(fp,"emails_window_width: %d\n",emails_window_width);
    fprintf(fp,"orient_right: %d\n", orient_right);
	fprintf(fp,"selected_style_index: %d\n", selected_style_index);
	fprintf(fp,"selected_font_index: %d\n", selected_font_index);
	fprintf(fp,"downloading_enabled: %d\n", is_automatic_downloading_enabled);
	fprintf(fp,"downloading_frequency: %d\n", automatic_downloading_freq);
	fprintf(fp,"downloading_pref_min: %d\n", automatic_downloading_pref_min);
	fprintf(fp,"downloading_pref_hour: %d\n", automatic_downloading_pref_hour);
	fprintf(fp,"downloading_pref_day: %d\n", automatic_downloading_pref_day);
	fprintf(fp,"downloading_pref_dow: %d\n", automatic_downloading_pref_dow);


    fclose(fp);
}

static void load_config()
{
    FILE *fp = fopen(config_file_path,"r");

	if (fp == NULL)
		return;

    char key[100] = {0};
    char val[2000] = {0};

    int index = 0;

    bool is_key = true;

    for(;;)
    {
        char c = fgetc(fp);

        if(c == EOF)
            break;

        switch(c)
        {
            case  '#':
                // comment
                // read until eol or eof
                for(;;)
                {
                    c = fgetc(fp);

                    if (c == EOF || c == '\n')
                        break;
                }
                
                break;
            case  ':': 
				if (!is_key)
				{
					val[index++] = c;
					break;
				}
					

                is_key = false;
                index = 0;
                
                // skip all whitespace after colon
                for(;;)
                {
                    c = fgetc(fp);

                    if (c == EOF || c == '\n' || !(c == ' ' || c == '\r' || c == '\t'))
                    {
                        // move back one
                        fseek( fp, -1, SEEK_CUR);
                        break; // no more whitespace
                    }
                }
                break;
			case '\r':
				// ignore on Windows
				break;
            case '\n':
                is_key = true;
                index = 0;

                int iVal;

                if(strcmp(key,"root_dir")  == 0) {strncpy(root_dir,val,strlen(val));}
                else if(strcmp(key,"orient_right")  == 0) {iVal = atoi(val); orient_right = iVal;}
                else if(strcmp(key,"xls_override")  == 0) {iVal = atoi(val); xls_override = iVal;}
                else if(strcmp(key,"doc_override")  == 0) {iVal = atoi(val); doc_override = iVal;}
                else if(strcmp(key,"csv_override")  == 0) {iVal = atoi(val); csv_override = iVal;}
                else if(strcmp(key,"txt_override")  == 0) {iVal = atoi(val); txt_override = iVal;}
                else if(strcmp(key,"pdf_override")  == 0) {iVal = atoi(val); pdf_override = iVal;}
                else if(strcmp(key,"img_override")  == 0) {iVal = atoi(val); img_override = iVal;}
                else if(strcmp(key,"misc_override") == 0) {iVal = atoi(val); misc_override = iVal;}
                else if(strcmp(key,"xls_program_path")  == 0) {strncpy(xls_program_path,val, strlen(val));}
                else if(strcmp(key,"doc_program_path")  == 0) {strncpy(doc_program_path,val, strlen(val));}
                else if(strcmp(key,"csv_program_path")  == 0) {strncpy(csv_program_path,val, strlen(val));}
                else if(strcmp(key,"txt_program_path")  == 0) {strncpy(txt_program_path,val, strlen(val));}
                else if(strcmp(key,"pdf_program_path")  == 0) {strncpy(pdf_program_path,val, strlen(val));}
                else if(strcmp(key,"img_program_path")  == 0) {strncpy(img_program_path,val, strlen(val));}
                else if(strcmp(key,"misc_program_path") == 0) {strncpy(misc_program_path,val, strlen(val));}
                else if(strcmp(key,"export_program_path")   == 0) {strncpy(export_program_path,val, strlen(val));}
                else if(strcmp(key,"emails_window_width")   == 0) {iVal = atoi(val); emails_window_width = iVal;}
                else if(strcmp(key,"selected_style_index")  == 0) {iVal = atoi(val); selected_style_index = iVal;}
                else if(strcmp(key,"selected_font_index")   == 0) {iVal = atoi(val); selected_font_index = iVal;}
                else if(strcmp(key,"downloading_enabled")   == 0) {iVal = atoi(val); is_automatic_downloading_enabled = iVal;}
                else if(strcmp(key,"downloading_frequency") == 0) {iVal = atoi(val); automatic_downloading_freq = iVal;}
                else if(strcmp(key,"downloading_pref_min")  == 0) {iVal = atoi(val); automatic_downloading_pref_min = iVal;}
                else if(strcmp(key,"downloading_pref_hour") == 0) {iVal = atoi(val); automatic_downloading_pref_hour = iVal;}
                else if(strcmp(key,"downloading_pref_day")  == 0) {iVal = atoi(val); automatic_downloading_pref_day = iVal;}
                else if(strcmp(key,"downloading_pref_dow")  == 0) {iVal = atoi(val); automatic_downloading_pref_dow = iVal;}

                memset(key,0,100);
                memset(val,0,2000);
                break;
            default:
                // valid character
                if(is_key)
                    key[index++] = c;
                else
                    val[index++] = c;

               break;
        }
    }
}

static void clear_email_memory()
{
    for(int i = 0; i < num_emails; ++i)
    {
        if(emails[i].file_path != NULL) {free(emails[i].file_path); emails[i].file_path = NULL;}
        if(emails[i].to != NULL)        {free(emails[i].to);emails[i].to = NULL;}
        if(emails[i].to_formatted != NULL) {free(emails[i].to_formatted);emails[i].to_formatted = NULL;}
        if(emails[i].cc != NULL)        {free(emails[i].cc);emails[i].cc = NULL;}
        if(emails[i].from != NULL)      {free(emails[i].from);emails[i].from = NULL;}
        if(emails[i].from_formatted != NULL) {free(emails[i].from_formatted);emails[i].from_formatted = NULL;}
        if(emails[i].forwarded != NULL) {free(emails[i].forwarded);emails[i].forwarded = NULL;}
        if(emails[i].date != NULL)      {free(emails[i].date);emails[i].date = NULL;}
        if(emails[i].date_formatted != NULL) { free(emails[i].date_formatted);emails[i].date_formatted = NULL;}
        if(emails[i].subject != NULL)   {free(emails[i].subject);emails[i].subject = NULL;}
        if(emails[i].body != NULL)      {free(emails[i].body);emails[i].body = NULL;}

        for(int j = 0; j < emails[i].attachment_count; ++j)
        {
            if (emails[i].attachments[j] != NULL) {free(emails[i].attachments[j]);emails[i].attachments[j] = NULL;}
            if (emails[i].attachments_ext[j] != NULL) {free(emails[i].attachments_ext[j]);emails[i].attachments_ext[j] = NULL;}
        }

		if (emails[i].attachments != NULL) { free(emails[i].attachments); emails[i].attachments = NULL;}
		if (emails[i].attachments_ext != NULL) { free(emails[i].attachments_ext); emails[i].attachments_ext = NULL; }

        emails[i].attachment_count = 0;
    }

	if (filtered_indicies != NULL) {free(filtered_indicies);filtered_indicies = NULL;}
	if (sorted_indicies   != NULL) {free(sorted_indicies);sorted_indicies = NULL;}

}

static void delete_email(int index)
{
    num_emails--;
    emails[index] = emails[num_emails];
}

static void export_emails_with_pass_background(bool show, bool wait, const char* pass)
{
    char command[260] = {0};

    strncpy(command,export_program_path,strlen(export_program_path));
    strcat(command," ");
    strcat(command,root_dir);
    strcat(command," -p ");
    strcat(command,pass);
    strcat(command," -q");
    strcat(command," -b");

    start_process(command,show, wait);
}
static void export_emails_with_pass(bool show, bool wait, const char* pass)
{
    char command[260] = {0};

    strncpy(command,export_program_path,strlen(export_program_path));
    strcat(command," ");
    strcat(command,root_dir);
    strcat(command," -p ");
    strcat(command,pass);

    start_process(command,show, wait);
}

static void export_emails(bool show, bool wait)
{
    char command[260] = {0};

    strncpy(command,export_program_path,strlen(export_program_path));
    strcat(command," ");
    strcat(command,root_dir);

    start_process(command,show,wait);
}

static void get_emails(const char* directory_path)
{
    DIR *dir;
    struct dirent *dp;

    dir = opendir(directory_path);

    if(dir == NULL)
        return;
    
    for(;;)
    {
        dp = readdir(dir);

        if(dp == NULL)
            break;

		if (strcmp(dp->d_name,".") == 0 || strcmp(dp->d_name,"..") == 0)
			continue;

        char full_path[MAX_PATH] = {0};
        strncpy(full_path,directory_path,strlen(directory_path));
        strcat(full_path,"\\");
        strcat(full_path,dp->d_name);

        if(str_startswith(dp->d_name,"email_"))
        {
            emails[num_emails].file_path = (char*)calloc(strlen(full_path)+1,sizeof(char));
            strncpy(emails[num_emails].file_path,full_path,strlen(full_path));
            //parse_email(full_path, num_emails);
			++num_emails;
        }

        get_emails(full_path);
    }

    closedir(dir);
}

static char* get_current_datetime(struct tm &tstruct)
{
	time_t     now = time(0);
	char       buf[20] = {0};
	tstruct = *localtime(&now);
	strftime(buf, sizeof(buf), "%Y-%m-%d.%X", &tstruct);

	return buf;
}

static void get_next_download_datetime(char* download_datetime)
{
    struct tm current_tstruct;
    struct tm next_download_tstruct;

    // get current time string of form: yyyy-MM-dd.HH:mm:ss
    char* current_datetime = get_current_datetime(current_tstruct);

    // get next download time string
    next_download_tstruct.tm_sec   = current_tstruct.tm_sec;
    next_download_tstruct.tm_min   = current_tstruct.tm_min;
    next_download_tstruct.tm_hour  = current_tstruct.tm_hour;
    next_download_tstruct.tm_mday  = current_tstruct.tm_mday;
    next_download_tstruct.tm_mon   = current_tstruct.tm_mon;
    next_download_tstruct.tm_year  = current_tstruct.tm_year;
    next_download_tstruct.tm_wday  = current_tstruct.tm_wday;
    next_download_tstruct.tm_yday  = current_tstruct.tm_yday;
    next_download_tstruct.tm_isdst = current_tstruct.tm_isdst;

	int num_sec_current = 0;
	int num_sec_next = 0;

	switch (automatic_downloading_freq)
	{
	case 0: 

        next_download_tstruct.tm_min = automatic_downloading_pref_min;
		next_download_tstruct.tm_sec = 0;

        num_sec_current = 60*current_tstruct.tm_min + current_tstruct.tm_sec;
        num_sec_next    = 60*next_download_tstruct.tm_min;

        // check to see if current minute is greater than download minute
        if(num_sec_current >= num_sec_next)
        {
            // need to increment hour
            next_download_tstruct.tm_hour += 1;

            if(next_download_tstruct.tm_hour > 23) {

                next_download_tstruct.tm_hour = 0;
                next_download_tstruct.tm_mday += 1;

                if(next_download_tstruct.tm_mday > 31)
                {
                    next_download_tstruct.tm_mday = 1;
                    next_download_tstruct.tm_mon  += 1;

                    if(next_download_tstruct.tm_mon > 11)
                    {
                        next_download_tstruct.tm_mon = 0;
                        next_download_tstruct.tm_year += 1;
                    }
                }
            }
        }
        break; // every hour
	case 1:

        next_download_tstruct.tm_hour = automatic_downloading_pref_hour;
        next_download_tstruct.tm_min = automatic_downloading_pref_min;
		next_download_tstruct.tm_sec = 0;

        num_sec_current = 3600*current_tstruct.tm_hour       + 60*current_tstruct.tm_min + current_tstruct.tm_sec;
        num_sec_next    = 3600*next_download_tstruct.tm_hour + 60*next_download_tstruct.tm_min;

        // check to see if current minute is greater than download minute
        if(num_sec_current >= num_sec_next)
        {
            // increment day
            next_download_tstruct.tm_mday += 1;

            if(next_download_tstruct.tm_mday > 31)
            {
                next_download_tstruct.tm_mday = 1;
                next_download_tstruct.tm_mon  += 1;

                if(next_download_tstruct.tm_mon > 11)
                {
                    next_download_tstruct.tm_mon = 0;
                    next_download_tstruct.tm_year += 1;
                }
            }
        }
        
        break; // every day
	case 2:
        // U M T W H F S
        // 0 1 2 3 4 5 6
        // @TODO: Not done yet...
        
        next_download_tstruct.tm_wday = automatic_downloading_pref_dow;
        next_download_tstruct.tm_mday += (next_download_tstruct.tm_wday - current_tstruct.tm_wday);
        next_download_tstruct.tm_hour = automatic_downloading_pref_hour;
        next_download_tstruct.tm_min  = automatic_downloading_pref_min;
		next_download_tstruct.tm_sec  = 0;

        num_sec_current = 86400*current_tstruct.tm_mday + 3600*current_tstruct.tm_hour       + 60*current_tstruct.tm_min + current_tstruct.tm_sec;
        num_sec_next    = 86400*current_tstruct.tm_mday + 3600*next_download_tstruct.tm_hour + 60*next_download_tstruct.tm_min;

        // check to see if current minute is greater than download minute
        if(num_sec_current >= num_sec_next)
        {
            next_download_tstruct.tm_mday += 1;

            if(next_download_tstruct.tm_mday > 31)
            {
                next_download_tstruct.tm_mday = 1;
                next_download_tstruct.tm_mon  += 1;

                if(next_download_tstruct.tm_mon > 11)
                {
                    next_download_tstruct.tm_mon = 0;
                    next_download_tstruct.tm_year += 1;
                }
            }
        }
        
        break; // every week
	case 3: break; // every month

        next_download_tstruct.tm_mday = automatic_downloading_pref_day;
        next_download_tstruct.tm_hour = automatic_downloading_pref_hour;
        next_download_tstruct.tm_min  = automatic_downloading_pref_min;
		next_download_tstruct.tm_sec  = 0;

        num_sec_current = 86400*current_tstruct.tm_mday       + 3600*current_tstruct.tm_hour       + 60*current_tstruct.tm_min + current_tstruct.tm_sec;
        num_sec_next    = 86400*next_download_tstruct.tm_mday + 3600*next_download_tstruct.tm_hour + 60*next_download_tstruct.tm_min;

        // check to see if current minute is greater than download minute
        if(num_sec_current >= num_sec_next)
        {
            // increment month
            next_download_tstruct.tm_mon  += 1;

            if(next_download_tstruct.tm_mon > 11)
            {
                next_download_tstruct.tm_mon = 0;
                next_download_tstruct.tm_year += 1;
            }
        }

    case 4:
            // this will make the download run continuously
            break;
	}

	strftime(download_datetime, 20, "%Y-%m-%d.%X", &next_download_tstruct);
}

static char* get_encryption_key()
{
    char user_name[BIN_LENGTH + 1] = {0};
    char key[BIN_LENGTH + 1]       = {0};

    strncpy(key,"zPe&4GoFaQitU3Pk-FEeU7e*k)JrmgZZ",BIN_LENGTH);

	DWORD size = 32;
	LPDWORD lpd = &size;
    if(GetUserNameA(user_name, lpd))
	{
		int user_name_len = strlen(user_name);
		for (int i = 0; i < user_name_len;++i)
		{
			key[i] = user_name[i];
		}
	}

	OutputDebugStringA(user_name);

    return key;
}

static char* encrypt_password(char* password)
{
	char pass[BIN_LENGTH + 1] = { 0 };
	strncpy(pass, password, BIN_LENGTH);

	char key[BIN_LENGTH + 1] = { 0 };
	strncpy(key, get_encryption_key(), BIN_LENGTH);

    char encrypted_pass[BIN_LENGTH + 1] = {0};

    int len = strlen(pass);

    for(int i = 0; i < BIN_LENGTH; ++i)
    {
		if (i < len)
		{
			encrypted_pass[i] = pass[i] ^ key[i];
		}
		else
		{
			encrypted_pass[i] = '\0';
		}

		OutputDebugStringA(encrypted_pass);
		OutputDebugStringA("\n");
    }

    encrypted_pass[BIN_LENGTH] = '\0';

    return encrypted_pass;
}

static char* decrypt_password(char* encrypted_password)
{
	if (strlen(encrypted_password) > BIN_LENGTH)
		return "";

	char key[BIN_LENGTH + 1] = { 0 };
	strncpy(key, get_encryption_key(), BIN_LENGTH);

    char password[BIN_LENGTH + 1] = {0};

	int encrypted_password_length = strlen(encrypted_password);
    int pass_length = 0;
	for (int i = 0; i < encrypted_password_length; ++i)
	{
		char tmp = encrypted_password[pass_length] ^ key[i % encrypted_password_length];
		password[i] = tmp;
		++pass_length;
	}

    return password;

}

static bool time_to_download()
{
    struct tm current_tstruct;

    char* current_datetime = get_current_datetime(current_tstruct);
    int   compare_result   = strcmp(download_datetime,current_datetime);

    if(compare_result < 0)
    {
        // current_datetime is past download_datetime!
        get_next_download_datetime(download_datetime);
    }
    return (compare_result == 0);
}

static void download_emails_and_refresh()
{
    while (msg.message != WM_QUIT)
    {
        if (is_automatic_downloading_enabled)
        {
            if (time_to_download())
            {
                export_emails_with_pass_background(false,true,notes_password);
                write_direct_to_cache(root_dir);
                get_next_download_datetime(download_datetime);
            }

        }
        Sleep(200);
    }
}

static void display_main_menu()
{
	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("Quit", "Alt+F4"))
				PostQuitMessage(0);
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Settings"))
		{
            if(ImGui::MenuItem("Edit Settings..."))
            {
                show_settings_window = !show_settings_window;
            }

            ImGui::Separator();
			ImGui::DragInt("Email List Width (px)", &emails_window_width, 1);
			ImGui::Checkbox("Orient Right",&orient_right);
			ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Theme"))
        {
            ImGuiIO& io = ImGui::GetIO();
            ImFont* font_current = ImGui::GetFont();
            show_style_selector("Style");

            if (ImGui::BeginCombo("Font", font_current->GetDebugName()))
            {
				for (int n = 0; n < io.Fonts->Fonts.Size; n++)
				{
					if (ImGui::Selectable(io.Fonts->Fonts[n]->GetDebugName(), io.Fonts->Fonts[n] == font_current))
					{
						io.FontDefault = io.Fonts->Fonts[n];
						selected_font_index = n;
					}
						
				}

                ImGui::EndCombo();
            }
            ImGui::EndMenu();
        }
		if (ImGui::BeginMenu("Emails##menu"))
		{
            if (ImGui::MenuItem("Open Emails Folder..."))
            {
                char command[300] = {0};

                strncpy(command,"C:\\Windows\\SysWOW64\\explorer.exe",32);
                strcat(command," \"");
                strcat(command,root_dir);
                strcat(command,"\"");
                start_process(command);
            }

            ImGui::Separator();

            if (ImGui::BeginMenu("Automatic Downloading of Emails"))
            {
                ImGui::Checkbox("Enable Downloading",&is_automatic_downloading_enabled);

				if (is_automatic_downloading_enabled)
				{
					ImGui::Separator();
					ImGui::RadioButton("Every Hour",  &automatic_downloading_freq, 0);
					ImGui::RadioButton("Every Day",   &automatic_downloading_freq, 1);
					ImGui::RadioButton("Every Week",  &automatic_downloading_freq, 2);
					ImGui::RadioButton("Every Month", &automatic_downloading_freq, 3);
					ImGui::Separator();

					if (automatic_downloading_freq == 3) ImGui::SliderInt("Preferred Day", &automatic_downloading_pref_day, 1, 31);
					else if (automatic_downloading_freq == 2)
					{
						ImGui::ListBox("Day of Week", &automatic_downloading_pref_dow, dow_items, IM_ARRAYSIZE(dow_items), 4);
					}
					if (automatic_downloading_freq >= 1) ImGui::SliderInt("Preferred Hour", &automatic_downloading_pref_hour, 1, 24);
					ImGui::SliderInt("Preferred Minute", &automatic_downloading_pref_min, 0, 59);

					char pref_label[20] = { 0 };

                    switch (automatic_downloading_freq)
                    {
                        case 0: sprintf(pref_label, ":%02d", automatic_downloading_pref_min); break; // every hour
                        case 1: sprintf(pref_label, "%02d:%02d", automatic_downloading_pref_hour, automatic_downloading_pref_min); break; // every day
                        case 2: sprintf(pref_label, "%s %02d:%02d", dow_items[automatic_downloading_pref_dow],automatic_downloading_pref_hour, automatic_downloading_pref_min); break; // every week
                        case 3: sprintf(pref_label, "Day %d on %02d:%02d", automatic_downloading_pref_day ,automatic_downloading_pref_hour, automatic_downloading_pref_min); break; // every month
                    }
					
					ImGui::Text(pref_label);
                    ImGui::Separator();

                    ImGui::InputText("Notes Password", notes_password, 100,pass_flags); 
                    ImGui::SameLine();
                    ImGui::Text("(<o>)");
                    if (ImGui::IsItemActive() || ImGui::IsItemHovered())
                        pass_flags = 0;
                    else
                        pass_flags = ImGuiInputTextFlags_Password | ImGuiInputTextFlags_CharsNoBlank;

                    ImGui::Text("*A password is required for automatic downloading.\nThis password will be encrypted and stored in your\nlocal user folder.");
            
                    // @CLEANUP: Should only do this when a propery has changed instead of every frame
                    get_next_download_datetime(download_datetime);
                }
               
                ImGui::EndMenu();
            }
            ImGui::Separator();

            if (ImGui::MenuItem("Download Emails From IBM Notes..."))
            {
                if(strcmp(notes_password,"") == 0)
                    export_emails(true,false); // manually ask for password
                else
                    export_emails_with_pass(true,false,notes_password); // feed password in
            }

			if (ImGui::Button("Refresh Emails..."))
                ImGui::OpenPopup("Refresh Emails");
            if (ImGui::BeginPopupModal("Refresh Emails", NULL, ImGuiWindowFlags_AlwaysAutoResize))
            {
                ImGui::Text("This will reload all of your emails\nand may take some time!\n\n");
                ImGui::Separator();

                if (ImGui::Button("OK, Do it", ImVec2(120,0)))
                {
                    reset_and_load_emails();
                    ImGui::CloseCurrentPopup(); 
                }
                ImGui::SameLine();
                if (ImGui::Button("Cancel", ImVec2(120,0))) { ImGui::CloseCurrentPopup(); }
                ImGui::EndPopup();
            }
			ImGui::EndMenu();

		}
		if (ImGui::BeginMenu("About"))
		{
			ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
			ImGui::Separator();
			ImGui::Text("-Notes Attic- V%d.%d",VERSION_MAJOR,VERSION_MINOR);
			ImGui::Text("Secondary Storage for IBM Notes Emails");
			ImGui::Text("By: Chris Rose [czr]");
			ImGui::EndMenu();
		}
		ImGui::EndMainMenuBar();
	}
}

static bool show_style_selector(const char* label)
{
    if (ImGui::Combo(label, &selected_style_index, "Classic\0Dark\0Light\0Monochrome\0Ecstacy\0Minimal\0Gruvbox_Dark\0Gruvbox_Light\0Gumby"))
    {
		switch (selected_style_index)
        {
        case 0: ImGui::StyleColorsClassic(); header_color = ImVec4(0.4f,0.2f,0.8f,1.0f);break;
        case 1: ImGui::StyleColorsDark();    header_color = ImVec4(0.4f,0.5f,0.7f,1.0f);break;
        case 2: ImGui::StyleColorsLight();   header_color = ImVec4(0.2f,0.4f,0.9f,1.0f);break;
        case 3: load_style_monochrome();     header_color = ImVec4(0.07f,0.34f,0.34f,1.0f);break;
        case 4: load_style_ecstacy();        header_color = ImVec4(1.0f,1.0f,1.0f,1.0f);break;
        case 5: load_style_minimal();        header_color = ImVec4(0.4f,0.4f,0.4f,1.0f);break;
        case 6: load_style_gruvbox();        header_color = ImVec4(0.4f,0.4f,0.4f,1.0f);break;
        case 7: load_style_gruvbox_light();  header_color = ImVec4(0.4f,0.4f,0.4f,1.0f);break;
        case 8: load_style_gumby();          header_color = ImVec4(0.4f,0.4f,0.4f,1.0f);break;
        }
        return true;
    }
    return false;
}

static void load_style_ecstacy()
{
	ImGuiStyle * style = &ImGui::GetStyle();

	//style->WindowPadding = ImVec2(15, 15);
	style->WindowRounding = 5.0f;
	//style->FramePadding = ImVec2(5, 5);
	style->FrameRounding = 4.0f;
	//style->ItemSpacing = ImVec2(12, 8);
	//style->ItemInnerSpacing = ImVec2(8, 6);
	//style->IndentSpacing = 25.0f;
	//style->ScrollbarSize = 15.0f;
	//style->ScrollbarRounding = 9.0f;
	style->GrabMinSize = 5.0f;
	style->GrabRounding = 3.0f;

	style->Colors[ImGuiCol_Text] = ImVec4(0.80f, 0.80f, 0.83f, 1.00f);
	style->Colors[ImGuiCol_TextDisabled] = ImVec4(0.24f, 0.23f, 0.29f, 1.00f);
	style->Colors[ImGuiCol_WindowBg] = ImVec4(0.06f, 0.05f, 0.07f, 1.00f);
	style->Colors[ImGuiCol_ChildWindowBg] = ImVec4(0.07f, 0.07f, 0.09f, 1.00f);
	style->Colors[ImGuiCol_PopupBg] = ImVec4(0.07f, 0.07f, 0.09f, 1.00f);
	style->Colors[ImGuiCol_Border] = ImVec4(0.80f, 0.80f, 0.83f, 0.88f);
	style->Colors[ImGuiCol_BorderShadow] = ImVec4(0.92f, 0.91f, 0.88f, 0.00f);
	style->Colors[ImGuiCol_FrameBg] = ImVec4(0.10f, 0.09f, 0.12f, 1.00f);
	style->Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.24f, 0.23f, 0.29f, 1.00f);
	style->Colors[ImGuiCol_FrameBgActive] = ImVec4(0.56f, 0.56f, 0.58f, 1.00f);
	style->Colors[ImGuiCol_TitleBg] = ImVec4(0.10f, 0.09f, 0.12f, 1.00f);
	style->Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(1.00f, 0.98f, 0.95f, 0.75f);
	style->Colors[ImGuiCol_TitleBgActive] = ImVec4(0.07f, 0.07f, 0.09f, 1.00f);
	style->Colors[ImGuiCol_MenuBarBg] = ImVec4(0.10f, 0.09f, 0.12f, 1.00f);
	style->Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.10f, 0.09f, 0.12f, 1.00f);
	style->Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.80f, 0.80f, 0.83f, 0.31f);
	style->Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.56f, 0.56f, 0.58f, 1.00f);
	style->Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.06f, 0.05f, 0.07f, 1.00f);
	style->Colors[ImGuiCol_ChildBg] = ImVec4(0.19f, 0.18f, 0.21f, 1.00f);
	style->Colors[ImGuiCol_CheckMark] = ImVec4(0.80f, 0.80f, 0.83f, 0.31f);
	style->Colors[ImGuiCol_SliderGrab] = ImVec4(0.80f, 0.80f, 0.83f, 0.31f);
	style->Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.06f, 0.05f, 0.07f, 1.00f);
	style->Colors[ImGuiCol_Button] = ImVec4(0.10f, 0.09f, 0.12f, 1.00f);
	style->Colors[ImGuiCol_ButtonHovered] = ImVec4(0.24f, 0.23f, 0.29f, 1.00f);
	style->Colors[ImGuiCol_ButtonActive] = ImVec4(0.56f, 0.56f, 0.58f, 1.00f);
	style->Colors[ImGuiCol_Header] = ImVec4(0.10f, 0.09f, 0.12f, 1.00f);
	style->Colors[ImGuiCol_HeaderHovered] = ImVec4(0.56f, 0.56f, 0.58f, 1.00f);
	style->Colors[ImGuiCol_HeaderActive] = ImVec4(0.06f, 0.05f, 0.07f, 1.00f);
	style->Colors[ImGuiCol_Column] = ImVec4(0.56f, 0.56f, 0.58f, 1.00f);
	style->Colors[ImGuiCol_ColumnHovered] = ImVec4(0.24f, 0.23f, 0.29f, 1.00f);
	style->Colors[ImGuiCol_ColumnActive] = ImVec4(0.56f, 0.56f, 0.58f, 1.00f);
	style->Colors[ImGuiCol_ResizeGrip] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	style->Colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.56f, 0.56f, 0.58f, 1.00f);
	style->Colors[ImGuiCol_ResizeGripActive] = ImVec4(0.06f, 0.05f, 0.07f, 1.00f);
	style->Colors[ImGuiCol_CloseButton] = ImVec4(0.40f, 0.39f, 0.38f, 0.16f);
	style->Colors[ImGuiCol_CloseButtonHovered] = ImVec4(0.40f, 0.39f, 0.38f, 0.39f);
	style->Colors[ImGuiCol_CloseButtonActive] = ImVec4(0.40f, 0.39f, 0.38f, 1.00f);
	style->Colors[ImGuiCol_PlotLines] = ImVec4(0.40f, 0.39f, 0.38f, 0.63f);
	style->Colors[ImGuiCol_PlotLinesHovered] = ImVec4(0.25f, 1.00f, 0.00f, 1.00f);
	style->Colors[ImGuiCol_PlotHistogram] = ImVec4(0.40f, 0.39f, 0.38f, 0.63f);
	style->Colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.25f, 1.00f, 0.00f, 1.00f);
	style->Colors[ImGuiCol_TextSelectedBg] = ImVec4(0.25f, 1.00f, 0.00f, 0.43f);
	style->Colors[ImGuiCol_ModalWindowDarkening] = ImVec4(1.00f, 0.98f, 0.95f, 0.73f);
}

static void load_style_minimal()
{
	ImGuiStyle * style = &ImGui::GetStyle();

	ImVec4 black = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
	ImVec4 white = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
	ImVec4 grey = ImVec4(0.4f, 0.4f, 0.4f, 1.0f);
	ImVec4 dark_grey = ImVec4(0.12f, 0.12f, 0.12f, 1.0f);

	//style->WindowPadding = ImVec2(15, 15);
	style->WindowRounding = 0.0f;
	//style->FramePadding = ImVec2(5, 5);
	style->FrameRounding = 0.0f;
	//style->ItemSpacing = ImVec2(12, 8);
	//style->ItemInnerSpacing = ImVec2(8, 6);
	//style->IndentSpacing = 25.0f;
	//style->ScrollbarSize = 15.0f;
	//style->ScrollbarRounding = 9.0f;
	style->GrabMinSize = 5.0f;
	style->GrabRounding = 0.0f;

	style->Colors[ImGuiCol_Text] = white;
	style->Colors[ImGuiCol_TextDisabled] = grey;
	style->Colors[ImGuiCol_WindowBg] = black;
	style->Colors[ImGuiCol_ChildWindowBg] = dark_grey;
	style->Colors[ImGuiCol_PopupBg] = black;
	style->Colors[ImGuiCol_Border] = dark_grey;
	style->Colors[ImGuiCol_BorderShadow] = black;
	style->Colors[ImGuiCol_FrameBg] = dark_grey;
	style->Colors[ImGuiCol_FrameBgHovered] = dark_grey;
	style->Colors[ImGuiCol_FrameBgActive] = dark_grey;
	style->Colors[ImGuiCol_TitleBg] = dark_grey;
	style->Colors[ImGuiCol_TitleBgCollapsed] = dark_grey;
	style->Colors[ImGuiCol_TitleBgActive] = dark_grey;
	style->Colors[ImGuiCol_MenuBarBg] = dark_grey;
	style->Colors[ImGuiCol_ScrollbarBg] = dark_grey;
	style->Colors[ImGuiCol_ScrollbarGrab] = grey;
	style->Colors[ImGuiCol_ScrollbarGrabHovered] = grey;
	style->Colors[ImGuiCol_ScrollbarGrabActive] = grey;
	style->Colors[ImGuiCol_ChildBg] = dark_grey;
	style->Colors[ImGuiCol_CheckMark] = white;
	style->Colors[ImGuiCol_SliderGrab] = grey;
	style->Colors[ImGuiCol_SliderGrabActive] = grey;
	style->Colors[ImGuiCol_Button] = dark_grey;
	style->Colors[ImGuiCol_ButtonHovered] = grey;
	style->Colors[ImGuiCol_ButtonActive] = white;
	style->Colors[ImGuiCol_Header] = dark_grey;
	style->Colors[ImGuiCol_HeaderHovered] = grey;
	style->Colors[ImGuiCol_HeaderActive] = grey;
	style->Colors[ImGuiCol_Column] = grey;
	style->Colors[ImGuiCol_ColumnHovered] = grey;
	style->Colors[ImGuiCol_ColumnActive] = grey;
	style->Colors[ImGuiCol_ResizeGrip] = grey;
	style->Colors[ImGuiCol_ResizeGripHovered] = grey;
	style->Colors[ImGuiCol_ResizeGripActive] = grey;
	style->Colors[ImGuiCol_CloseButton] = grey;
	style->Colors[ImGuiCol_CloseButtonHovered] = grey;
	style->Colors[ImGuiCol_CloseButtonActive] = grey;
	style->Colors[ImGuiCol_PlotLines] = grey;
	style->Colors[ImGuiCol_PlotLinesHovered] = grey;
	style->Colors[ImGuiCol_PlotHistogram] = grey;
	style->Colors[ImGuiCol_PlotHistogramHovered] = grey;
	style->Colors[ImGuiCol_TextSelectedBg] = black;
	style->Colors[ImGuiCol_ModalWindowDarkening] = black;
}

static void load_style_monochrome()
{
    ImGuiStyle& style = ImGui::GetStyle();

    style.Alpha = 1.0;
    style.WindowRounding = 3;
    style.GrabRounding = 1;
    style.GrabMinSize = 20;
    style.FrameRounding = 3;

    style.Colors[ImGuiCol_Text] = ImVec4(0.00f, 1.00f, 1.00f, 1.00f);
    style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.00f, 0.40f, 0.41f, 1.00f);
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
    style.Colors[ImGuiCol_ChildWindowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	style.Colors[ImGuiCol_PopupBg] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
    style.Colors[ImGuiCol_Border] = ImVec4(0.00f, 1.00f, 1.00f, 0.65f);
    style.Colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    style.Colors[ImGuiCol_FrameBg] = ImVec4(0.44f, 0.80f, 0.80f, 0.18f);
    style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.44f, 0.80f, 0.80f, 0.27f);
    style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.44f, 0.81f, 0.86f, 0.66f);
    style.Colors[ImGuiCol_TitleBg] = ImVec4(0.14f, 0.18f, 0.21f, 0.73f);
    style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.00f, 0.00f, 0.00f, 0.54f);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.00f, 1.00f, 1.00f, 0.27f);
    style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.20f);
    style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.22f, 0.29f, 0.30f, 0.71f);
    style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.00f, 1.00f, 1.00f, 0.44f);
    style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.00f, 1.00f, 1.00f, 0.74f);
    style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.00f, 1.00f, 1.00f, 1.00f);
	style.Colors[ImGuiCol_ChildBg] = ImVec4(0.16f, 0.24f, 0.22f, 0.60f);
    style.Colors[ImGuiCol_CheckMark] = ImVec4(0.00f, 1.00f, 1.00f, 0.68f);
    style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.00f, 1.00f, 1.00f, 0.36f);
    style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.00f, 1.00f, 1.00f, 0.76f);
    style.Colors[ImGuiCol_Button] = ImVec4(0.00f, 0.65f, 0.65f, 0.46f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.01f, 1.00f, 1.00f, 0.43f);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.00f, 1.00f, 1.00f, 0.62f);
    style.Colors[ImGuiCol_Header] = ImVec4(0.00f, 1.00f, 1.00f, 0.33f);
    style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.00f, 1.00f, 1.00f, 0.42f);
    style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.00f, 1.00f, 1.00f, 0.54f);
    style.Colors[ImGuiCol_Column] = ImVec4(0.00f, 0.50f, 0.50f, 0.33f);
    style.Colors[ImGuiCol_ColumnHovered] = ImVec4(0.00f, 0.50f, 0.50f, 0.47f);
    style.Colors[ImGuiCol_ColumnActive] = ImVec4(0.00f, 0.70f, 0.70f, 1.00f);
    style.Colors[ImGuiCol_ResizeGrip] = ImVec4(0.00f, 1.00f, 1.00f, 0.54f);
    style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.00f, 1.00f, 1.00f, 0.74f);
    style.Colors[ImGuiCol_ResizeGripActive] = ImVec4(0.00f, 1.00f, 1.00f, 1.00f);
    style.Colors[ImGuiCol_CloseButton] = ImVec4(0.00f, 0.78f, 0.78f, 0.35f);
    style.Colors[ImGuiCol_CloseButtonHovered] = ImVec4(0.00f, 0.78f, 0.78f, 0.47f);
    style.Colors[ImGuiCol_CloseButtonActive] = ImVec4(0.00f, 0.78f, 0.78f, 1.00f);
    style.Colors[ImGuiCol_PlotLines] = ImVec4(0.00f, 1.00f, 1.00f, 1.00f);
    style.Colors[ImGuiCol_PlotLinesHovered] = ImVec4(0.00f, 1.00f, 1.00f, 1.00f);
    style.Colors[ImGuiCol_PlotHistogram] = ImVec4(0.00f, 1.00f, 1.00f, 1.00f);
    style.Colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.00f, 1.00f, 1.00f, 1.00f);
    style.Colors[ImGuiCol_TextSelectedBg] = ImVec4(0.00f, 1.00f, 1.00f, 0.22f);
    style.Colors[ImGuiCol_ModalWindowDarkening] = ImVec4(0.04f, 0.10f, 0.09f, 0.51f);

}

static void load_style_gruvbox()
{
    ImGuiStyle& style = ImGui::GetStyle();

	ImVec4 bg0    = ImVec4(0.157f, 0.157f, 0.157f, 1.000f);
	ImVec4 bg1    = ImVec4(0.235f, 0.220f, 0.212f, 1.000f);
	ImVec4 bg2    = ImVec4(0.314f, 0.286f, 0.271f, 1.000f);
	ImVec4 fg0    = ImVec4(0.984f, 0.945f, 0.780f, 1.000f);
	ImVec4 fg1    = ImVec4(0.922f, 0.859f, 0.698f, 1.000f);
	ImVec4 gray   = ImVec4(0.573f, 0.514f, 0.455f, 1.000f);
	ImVec4 red    = ImVec4(0.800f, 0.141f, 0.114f, 1.000f);
	ImVec4 yellow = ImVec4(0.843f, 0.600f, 0.129f, 1.000f);
	ImVec4 purple = ImVec4(0.694f, 0.384f, 0.525f, 1.000f);
	ImVec4 blue   = ImVec4(0.271f, 0.522f, 0.533f, 1.000f);
	ImVec4 aqua   = ImVec4(0.408f, 0.616f, 0.416f, 1.000f);
	ImVec4 green  = ImVec4(0.596f, 0.592f, 0.102f, 1.000f);

    style.Alpha          = 1.0;
    style.WindowRounding = 3;
    style.GrabRounding   = 1;
    style.GrabMinSize    = 20;
    style.FrameRounding  = 3;

    style.Colors[ImGuiCol_Text]                 = fg0;
    style.Colors[ImGuiCol_TextDisabled]         = gray;
    style.Colors[ImGuiCol_WindowBg]             = bg0;
    style.Colors[ImGuiCol_ChildWindowBg]        = bg0;
	style.Colors[ImGuiCol_PopupBg]              = bg0;
    style.Colors[ImGuiCol_Border]               = bg2;
    style.Colors[ImGuiCol_BorderShadow]         = bg1;
    style.Colors[ImGuiCol_FrameBg]              = bg1;
    style.Colors[ImGuiCol_FrameBgHovered]       = yellow;
    style.Colors[ImGuiCol_FrameBgActive]        = bg1;
    style.Colors[ImGuiCol_TitleBg]              = bg2;
    style.Colors[ImGuiCol_TitleBgCollapsed]     = bg2;
    style.Colors[ImGuiCol_TitleBgActive]        = bg2;
    style.Colors[ImGuiCol_MenuBarBg]            = bg0;
    style.Colors[ImGuiCol_ScrollbarBg]          = bg1;
    style.Colors[ImGuiCol_ScrollbarGrab]        = gray;
    style.Colors[ImGuiCol_ScrollbarGrabHovered] = blue;
    style.Colors[ImGuiCol_ScrollbarGrabActive]  = blue;
    style.Colors[ImGuiCol_ChildBg]              = bg0;
    style.Colors[ImGuiCol_CheckMark]            = blue;
    style.Colors[ImGuiCol_SliderGrab]           = gray;
    style.Colors[ImGuiCol_SliderGrabActive]     = blue;
    style.Colors[ImGuiCol_Button]               = blue;
    style.Colors[ImGuiCol_ButtonHovered]        = yellow;
    style.Colors[ImGuiCol_ButtonActive]         = yellow;
    style.Colors[ImGuiCol_Header]               = blue;
    style.Colors[ImGuiCol_HeaderHovered]        = yellow;
    style.Colors[ImGuiCol_HeaderActive]         = yellow;
    style.Colors[ImGuiCol_Column]               = bg0;
    style.Colors[ImGuiCol_ColumnHovered]        = bg0;
    style.Colors[ImGuiCol_ColumnActive]         = bg0;
    style.Colors[ImGuiCol_ResizeGrip]           = fg1;
    style.Colors[ImGuiCol_ResizeGripHovered]    = fg0;
    style.Colors[ImGuiCol_ResizeGripActive]     = yellow;
    style.Colors[ImGuiCol_CloseButton]          = red;
    style.Colors[ImGuiCol_CloseButtonHovered]   = yellow;
    style.Colors[ImGuiCol_CloseButtonActive]    = yellow;
    style.Colors[ImGuiCol_PlotLines]            = bg0;
    style.Colors[ImGuiCol_PlotLinesHovered]     = bg0;
    style.Colors[ImGuiCol_PlotHistogram]        = bg0;
    style.Colors[ImGuiCol_PlotHistogramHovered] = bg0;
    style.Colors[ImGuiCol_TextSelectedBg]       = yellow;
	style.Colors[ImGuiCol_ModalWindowDarkening] = ImVec4(0.157f, 0.157f, 0.157f, 0.700f);
}

static void load_style_gruvbox_light()
{
    ImGuiStyle& style = ImGui::GetStyle();

	ImVec4 bg0    = ImVec4(0.984f, 0.945f, 0.780f, 1.000f);
	ImVec4 bg1    = ImVec4(0.922f, 0.859f, 0.698f, 1.000f);
	ImVec4 bg2    = ImVec4(0.835f, 0.769f, 0.631f, 1.000f);
	ImVec4 fg0    = ImVec4(0.157f, 0.157f, 0.157f, 1.000f);
	ImVec4 fg1    = ImVec4(0.235f, 0.220f, 0.212f, 1.000f);
	ImVec4 gray   = ImVec4(0.573f, 0.514f, 0.455f, 1.000f);
	ImVec4 red    = ImVec4(0.616f, 0.000f, 0.024f, 1.000f);
	ImVec4 yellow = ImVec4(0.843f, 0.600f, 0.129f, 1.000f);
	ImVec4 purple = ImVec4(0.561f, 0.247f, 0.443f, 1.000f);
	ImVec4 blue   = ImVec4(0.271f, 0.522f, 0.533f, 1.000f);
	ImVec4 aqua   = ImVec4(0.259f, 0.482f, 0.345f, 1.000f);
	ImVec4 green  = ImVec4(0.475f, 0.455f, 0.055f, 1.000f);

    style.Alpha          = 1.0;
    style.WindowRounding = 3;
    style.GrabRounding   = 1;
    style.GrabMinSize    = 20;
    style.FrameRounding  = 3;

    style.Colors[ImGuiCol_Text]                 = fg0;
    style.Colors[ImGuiCol_TextDisabled]         = gray;
    style.Colors[ImGuiCol_WindowBg]             = bg1;
    style.Colors[ImGuiCol_ChildWindowBg]        = bg1;
	style.Colors[ImGuiCol_PopupBg]              = bg1;
    style.Colors[ImGuiCol_Border]               = bg2;
    style.Colors[ImGuiCol_BorderShadow]         = bg1;
    style.Colors[ImGuiCol_FrameBg]              = bg0;
    style.Colors[ImGuiCol_FrameBgHovered]       = gray;
    style.Colors[ImGuiCol_FrameBgActive]        = bg1;
    style.Colors[ImGuiCol_TitleBg]              = bg2;
    style.Colors[ImGuiCol_TitleBgCollapsed]     = bg2;
    style.Colors[ImGuiCol_TitleBgActive]        = bg2;
    style.Colors[ImGuiCol_MenuBarBg]            = bg0;
    style.Colors[ImGuiCol_ScrollbarBg]          = bg1;
    style.Colors[ImGuiCol_ScrollbarGrab]        = gray;
    style.Colors[ImGuiCol_ScrollbarGrabHovered] = yellow;
    style.Colors[ImGuiCol_ScrollbarGrabActive]  = yellow;
    style.Colors[ImGuiCol_ChildBg]              = bg0;
    style.Colors[ImGuiCol_CheckMark]            = yellow;
    style.Colors[ImGuiCol_SliderGrab]           = gray;
    style.Colors[ImGuiCol_SliderGrabActive]     = yellow;
    style.Colors[ImGuiCol_Button]               = bg2;
    style.Colors[ImGuiCol_ButtonHovered]        = yellow;
    style.Colors[ImGuiCol_ButtonActive]         = yellow;
    style.Colors[ImGuiCol_Header]               = bg1;
    style.Colors[ImGuiCol_HeaderHovered]        = yellow;
    style.Colors[ImGuiCol_HeaderActive]         = yellow;
    style.Colors[ImGuiCol_Column]               = bg0;
    style.Colors[ImGuiCol_ColumnHovered]        = bg0;
    style.Colors[ImGuiCol_ColumnActive]         = bg0;
    style.Colors[ImGuiCol_ResizeGrip]           = fg0;
    style.Colors[ImGuiCol_ResizeGripHovered]    = fg1;
    style.Colors[ImGuiCol_ResizeGripActive]     = blue;
    style.Colors[ImGuiCol_CloseButton]          = yellow;
    style.Colors[ImGuiCol_CloseButtonHovered]   = blue;
    style.Colors[ImGuiCol_CloseButtonActive]    = blue;
    style.Colors[ImGuiCol_PlotLines]            = bg0;
    style.Colors[ImGuiCol_PlotLinesHovered]     = bg0;
    style.Colors[ImGuiCol_PlotHistogram]        = bg0;
    style.Colors[ImGuiCol_PlotHistogramHovered] = bg0;
    style.Colors[ImGuiCol_TextSelectedBg]       = yellow;
	style.Colors[ImGuiCol_ModalWindowDarkening] = ImVec4(0.157f, 0.157f, 0.157f, 0.700f);
}

static void load_style_gumby()
{
    ImGuiStyle& style = ImGui::GetStyle();

	ImVec4 bg0    = ImVec4(0.118f, 0.118f, 0.118f, 1.000f);
	ImVec4 bg1    = ImVec4(0.050f, 0.050f, 0.050f, 1.000f);
	ImVec4 bg2    = ImVec4(0.333f, 0.443f, 0.337f, 1.000f);
	ImVec4 fg0    = ImVec4(0.502f, 0.878f, 0.478f, 1.000f);
	ImVec4 fg1    = ImVec4(0.843f, 0.843f, 0.843f, 1.000f);
	ImVec4 gray   = ImVec4(0.573f, 0.514f, 0.455f, 1.000f);
	ImVec4 green  = ImVec4(0.161f, 0.612f, 0.220f, 1.000f);

    style.Alpha          = 1.0;
    style.WindowRounding = 3;
    style.GrabRounding   = 1;
    style.GrabMinSize    = 20;
    style.FrameRounding  = 3;

    style.Colors[ImGuiCol_Text]                 = fg0;
    style.Colors[ImGuiCol_TextDisabled]         = gray;
    style.Colors[ImGuiCol_WindowBg]             = bg1;
    style.Colors[ImGuiCol_ChildWindowBg]        = bg1;
	style.Colors[ImGuiCol_PopupBg]              = bg1;
    style.Colors[ImGuiCol_Border]               = bg2;
    style.Colors[ImGuiCol_BorderShadow]         = bg1;
    style.Colors[ImGuiCol_FrameBg]              = bg0;
    style.Colors[ImGuiCol_FrameBgHovered]       = gray;
    style.Colors[ImGuiCol_FrameBgActive]        = bg1;
    style.Colors[ImGuiCol_TitleBg]              = bg2;
    style.Colors[ImGuiCol_TitleBgCollapsed]     = bg2;
    style.Colors[ImGuiCol_TitleBgActive]        = bg2;
    style.Colors[ImGuiCol_MenuBarBg]            = bg0;
    style.Colors[ImGuiCol_ScrollbarBg]          = bg1;
    style.Colors[ImGuiCol_ScrollbarGrab]        = gray;
    style.Colors[ImGuiCol_ScrollbarGrabHovered] = green;
    style.Colors[ImGuiCol_ScrollbarGrabActive]  = green;
    style.Colors[ImGuiCol_ChildBg]              = bg0;
    style.Colors[ImGuiCol_CheckMark]            = green;
    style.Colors[ImGuiCol_SliderGrab]           = gray;
    style.Colors[ImGuiCol_SliderGrabActive]     = green;
    style.Colors[ImGuiCol_Button]               = bg2;
    style.Colors[ImGuiCol_ButtonHovered]        = green;
    style.Colors[ImGuiCol_ButtonActive]         = green;
    style.Colors[ImGuiCol_Header]               = bg1;
    style.Colors[ImGuiCol_HeaderHovered]        = green;
    style.Colors[ImGuiCol_HeaderActive]         = green;
    style.Colors[ImGuiCol_Column]               = bg0;
    style.Colors[ImGuiCol_ColumnHovered]        = bg0;
    style.Colors[ImGuiCol_ColumnActive]         = bg0;
    style.Colors[ImGuiCol_ResizeGrip]           = fg0;
    style.Colors[ImGuiCol_ResizeGripHovered]    = fg1;
    style.Colors[ImGuiCol_ResizeGripActive]     = green;
    style.Colors[ImGuiCol_CloseButton]          = green;
    style.Colors[ImGuiCol_CloseButtonHovered]   = green;
    style.Colors[ImGuiCol_CloseButtonActive]    = green;
    style.Colors[ImGuiCol_PlotLines]            = bg0;
    style.Colors[ImGuiCol_PlotLinesHovered]     = bg0;
    style.Colors[ImGuiCol_PlotHistogram]        = bg0;
    style.Colors[ImGuiCol_PlotHistogramHovered] = bg0;
    style.Colors[ImGuiCol_TextSelectedBg]       = green;
	style.Colors[ImGuiCol_ModalWindowDarkening] = ImVec4(0.157f, 0.157f, 0.157f, 0.700f);
}

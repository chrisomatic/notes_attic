#pragma comment(linker, "/STACK:3000000")

#define MAX_PATH 260
#define FIELD_KEY_MAX 100
#define FIELD_VAL_MAX 10000
#define FIELD_BODY_MAX 100000

#include <imgui\imgui.h>

#include "imgui_impl_glfw_gl3.h"
#include <stdio.h>
#include <gl/gl3w.h>
#include <glfw/glfw3.h>
#include <dirent.h>
#include "process.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG 
#include "stb\stb_image.h"

typedef enum
{
    DATE,
    TO,
    FROM,
    SUBJECT,
    CC,
    FORWARDED,
    BODY
} EmailHeaderField;

#include "quicksort.h"

static void error_callback(int error, const char* description)
{
    fprintf(stderr, "Error %d: %s\n", error, description);
}

static int* filtered_indicies;
static int* sorted_indicies;

static int num_emails = 0;
static int num_emails_filtered = 0;

static int window_width;
static int window_height;
static float emails_window_width_pct = 0.33f;

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

static char xls_program_path[MAX_PATH]  = {0};
static char doc_program_path[MAX_PATH]  = {0};
static char csv_program_path[MAX_PATH]  = {0};
static char txt_program_path[MAX_PATH]  = {0};
static char pdf_program_path[MAX_PATH] = {0};
static char img_program_path[MAX_PATH] = {0};
static char misc_program_path[MAX_PATH] = {0};
static char export_program_path[MAX_PATH] = {0};

static char root_dir[MAX_PATH] = {0};
static bool show_settings_window = false;

static int selected_email_index = -1;
static int prev_selected_email_index = -1;
static ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse;

static GLFWwindow* window;
static const char* default_root_dir  = "C:\\email_archive";
static const char* default_xls_path  = "C:\\Program Files (x86)\\Microsoft Office\\OFFICE14\\EXCEL.EXE";
static const char* default_doc_path  = "C:\\Program Files (x86)\\Microsoft Office\\OFFICE14\\WINWORD.EXE";
static const char* default_csv_path  = "C:\\Windows\\system32\\notepad.exe";
static const char* default_txt_path  = "C:\\Windows\\system32\\notepad.exe";
static const char* default_pdf_path  = "C:\Program Files (x86)\Adobe\Acrobat Reader DC\Reader\AcroRd32.exe";
static const char* default_img_path  = "C:\\Windows\\system32\\rundll32.exe \"C:\\Program Files\\Windows Photo Viewer\\PhotoViewer.dll\", ImageView_Fullscreen";
static const char* default_misc_path = "C:\\Windows\\system32\\notepad.exe";

static const char* default_export_program_path = "exportnotes.exe";

static const char* config_folder     = "C:\\attic\\";
static const char* cache_file_path   = "C:\\attic\\attic.data";
static const char* config_file_path  = "C:\\attic\\attic.cfg";

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
	char* attachments[100];
    char* attachments_ext[100];
	int   attachment_count;
} NotesEmail;

static NotesEmail emails[1000000];
static NotesEmail curr_email;

static void parse_email(const char* file_path,int index);

static void get_emails(const char* directory_path);
static void export_emails();
static void display_main_menu();
static void apply_filters();
static void apply_sorting();
static void reset_and_load_emails();
static void load_emails();
static void store_cache();
static bool load_cache();
static void load_config();
static void store_config();
static void clear_email_memory();
static GLFWimage load_icon(const char* image_path);
static bool str_contains(char* base, char* search_pattern,bool case_sensitive);
static bool str_startswith(char* base, char* search_pattern);
static void str_tolower(char *str);
static char* str_getext(const char* file_path);
static char* dt_convert_to_sortable_format(const char* date);
static bool show_style_selector(const char* label);
static void load_style_monochrome();
static void load_style_ecstacy();
static void load_style_minimal();

int main(int, char**)
{

#ifdef _WIN32
	FreeConsole();
    CreateDirectory(config_folder,NULL);
#endif

    // Setup window
    glfwSetErrorCallback(error_callback);
    if (!glfwInit())
        return 1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#if __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_true);
#endif

	window_width = 1280;
	window_height = 800;

    window = glfwCreateWindow(window_width, window_height, "Notes Attic", NULL,NULL);

    // load icon
    GLFWimage images[1];
    images[0] = load_icon("attic.png");
    glfwSetWindowIcon(window, 1, images);

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync
    gl3wInit();

    // Setup ImGui binding
    ImGui_ImplGlfwGL3_Init(window, true);

	ImFontConfig config;
	config.OversampleH = 3;
	config.OversampleV = 1;
	config.GlyphExtraSpacing.x = 1.0f;

    ImGuiIO& io = ImGui::GetIO();
    //io.Fonts->AddFontDefault();
	io.Fonts->AddFontFromFileTTF("../../../fonts/Verdana.ttf", 15.0f, &config);
	io.Fonts->AddFontFromFileTTF("../../../fonts/OpenSans.ttf", 15.0f, &config);
	io.Fonts->AddFontFromFileTTF("../../../fonts/SegueUI.ttf", 15.0f, &config);
	io.Fonts->AddFontFromFileTTF("../../../fonts/Garamond.ttf", 15.0f, &config);
	io.Fonts->AddFontFromFileTTF("../../../fonts/Karla.ttf", 15.0f, &config);
    io.Fonts->AddFontFromFileTTF("../../../fonts/Roboto-Medium.ttf", 15.0f, &config);
	io.Fonts->AddFontFromFileTTF("../../../fonts/Inconsolata.ttf", 15.0f, &config);

    // load defaults
    strcpy(root_dir,default_root_dir);
    strcpy(xls_program_path,default_xls_path);
    strcpy(doc_program_path,default_doc_path);
    strcpy(csv_program_path,default_csv_path);
    strcpy(txt_program_path,default_txt_path);
    strcpy(pdf_program_path,default_pdf_path);
    strcpy(img_program_path,default_img_path);
    strcpy(misc_program_path,default_misc_path);
    strcpy(export_program_path,default_export_program_path);

    load_config();

	// Setup style
	io.FontDefault = io.Fonts->Fonts[selected_font_index];

	switch (selected_style_index)
	{
	case 0: ImGui::StyleColorsClassic(); header_color = ImVec4(0.4f,0.2f,0.8f,1.0f);break;
	case 1: ImGui::StyleColorsDark();    header_color = ImVec4(0.4f,0.5f,0.7f,1.0f);break;
	case 2: ImGui::StyleColorsLight();   header_color = ImVec4(0.2f,0.4f,0.9f,1.0f);break;
	case 3: load_style_monochrome();     header_color = ImVec4(0.07f,0.34f,0.34f,1.0f);break;
	case 4: load_style_ecstacy();        header_color = ImVec4(1.0f,1.0f,1.0f,1.0f);break;
	case 5: load_style_minimal();        header_color = ImVec4(0.4f,0.4f,0.4f,1.0f);break;
	}

	ImGuiStyle& style = ImGui::GetStyle();
	style.WindowRounding = 0;
	style.FrameRounding = 3;

    // try to load cache file
    if(!load_cache())
        load_emails();

    apply_filters();
    apply_sorting();

    ImVec4 clear_color = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);

    // Main loop
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
        ImGui_ImplGlfwGL3_NewFrame();

		display_main_menu();

		glfwGetWindowSize(window, &window_width, &window_height);

        float email_list_width = window_width*emails_window_width_pct;

        ImGui::SetNextWindowPos(ImVec2(0, 22), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(email_list_width, window_height - 22), ImGuiCond_Always);
        ImGui::Begin("Emails", 0,window_flags);

        int list_height_sub = 110;

        if (ImGui::CollapsingHeader("Filters",ImGuiTreeNodeFlags_DefaultOpen))
        {

            ImGui::InputText("To",filter_to,FIELD_VAL_MAX);
            ImGui::InputText("From",filter_from,FIELD_VAL_MAX);
            ImGui::InputText("Date", filter_date, FIELD_VAL_MAX);
            ImGui::InputText("Subject",filter_subject,FIELD_VAL_MAX);
            ImGui::InputText("Body",filter_body,FIELD_VAL_MAX);
            ImGui::Checkbox("Only Attachments",&only_attachments); ImGui::SameLine();

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
            list_height_sub += 175;
        }

        ImGui::Spacing();

        ImGui::PushItemWidth(60);
        if(ImGui::Combo("Dir", &sort_dir, "Asc\0Desc"))
            apply_sorting();
        ImGui::PopItemWidth();
        ImGui::SameLine();

        ImGui::PushItemWidth(window_width*emails_window_width_pct-140 - 60);
        if(ImGui::Combo("Sort Field", &sort_field, "Date\0To\0From\0Subject\0CC\0Forwarded"))
            apply_sorting();
        ImGui::PopItemWidth();

        ImGui::BeginChild(ImGui::GetID((void*)(intptr_t)0), ImVec2(ImGui::GetWindowWidth() - 28, ImGui::GetWindowHeight() -list_height_sub), true);

        /*
        ImGuiIO& io = ImGui::GetIO();
		if (io.KeysDown[GLFW_KEY_UP])
		{
			selected_email_index--;
			io.KeysDown[GLFW_KEY_UP] = false;
		}
		else if (io.KeysDown[GLFW_KEY_DOWN])
		{
			selected_email_index++;
			io.KeysDown[GLFW_KEY_DOWN] = false;
		}
        */
        for (int i = 0; i < num_emails_filtered; ++i)
        {
            char id[FIELD_VAL_MAX+10] = {0};
            NotesEmail e = emails[filtered_indicies[sorted_indicies[i]]];

			sprintf(id, "%s\n%s\n%s##%d",e.from, e.date, e.subject,i);

			if (e.attachment_count > 0)
				ImGui::TextColored(ImVec4(0.5f,0.5f,0.2f,1.0f),"@");
            else
				ImGui::Text(" ");

			ImGui::SameLine(25);
            if (ImGui::Selectable(id, (selected_email_index == i),0,ImVec2(ImGui::GetWindowWidth(),50)))
                    selected_email_index = i;
        }

        ImGui::EndChild();
        ImGui::Text("%d/%d",num_emails_filtered, num_emails);
        ImGui::End();

        // load email if needed
		if (selected_email_index > -1)
		{
			if (selected_email_index != prev_selected_email_index)
			{
				// parse and load email body
                NotesEmail e = emails[filtered_indicies[sorted_indicies[selected_email_index]]];

				for (int i = 0; i < e.attachment_count; ++i)
                {
					curr_email.attachments[i] = e.attachments[i];
					curr_email.attachments_ext[i] = e.attachments_ext[i];
                }
                
				// point current email at selected one
                curr_email.to = e.to;
                curr_email.cc = e.cc;
                curr_email.from = e.from;
                curr_email.forwarded = e.forwarded;
                curr_email.date = e.date;
                curr_email.subject = e.subject;
				curr_email.attachment_count = e.attachment_count;
                curr_email.body = e.body;

				prev_selected_email_index = selected_email_index;
			}
		}

        ImGui::SetNextWindowPos(ImVec2(window_width*emails_window_width_pct, 22), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(window_width*(1.0f - emails_window_width_pct), 148), ImGuiCond_Always);

        ImGui::Begin("Header", 0,window_flags);
		ImGui::TextColored(header_color, "To:"); ImGui::SameLine(80); ImGui::Text("%s", curr_email.to);
		ImGui::TextColored(header_color, "CC:"); ImGui::SameLine(80); ImGui::Text("%s", curr_email.cc);
		ImGui::TextColored(header_color, "From:"); ImGui::SameLine(80); ImGui::Text("%s", curr_email.from);
		ImGui::TextColored(header_color, "Fwd From:"); ImGui::SameLine(80); ImGui::Text("%s", curr_email.forwarded);
		ImGui::TextColored(header_color, "Date:"); ImGui::SameLine(80); ImGui::Text("%s", curr_email.date);
		ImGui::TextColored(header_color, "Subject:"); ImGui::SameLine(80); ImGui::Text("%s", curr_email.subject);
        ImGui::End();

        float body_width = window_width*(1.0f - emails_window_width_pct);

        ImGui::SetNextWindowPos(ImVec2(window_width*emails_window_width_pct, 170), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(body_width, window_height - 170 - 200), ImGuiCond_Always);
        ImGui::Begin("Body", 0,window_flags);

		/*
		if (ImGui::Button("Copy All Text"))
			ImGui::SetClipboardText(curr_email.body);
		*/

        if (curr_email.body == NULL)
            ImGui::InputTextMultiline("body", "", FIELD_BODY_MAX, ImVec2(ImGui::GetWindowWidth()-14, ImGui::GetWindowHeight() - 40),ImGuiInputTextFlags_ReadOnly);
        else
            ImGui::InputTextMultiline("body", curr_email.body, FIELD_BODY_MAX, ImVec2(ImGui::GetWindowWidth() - 14, ImGui::GetWindowHeight() - 40), ImGuiInputTextFlags_ReadOnly);
        ImGui::End();

        float attachments_width = window_width*(1.0f - emails_window_width_pct);

        ImGui::SetNextWindowPos(ImVec2(window_width*emails_window_width_pct,window_height - 200), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(attachments_width,200), ImGuiCond_Always);
        ImGui::Begin("Attachments", 0,window_flags);
        ImGui::Text("Attachment Count: %d", curr_email.attachment_count);

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
                    strcpy(full_attachment_path, root_dir);
                    strcat(full_attachment_path, curr_email.attachments[i]);
                }
                else
                    strcpy(full_attachment_path,curr_email.attachments[i]);

                if (strcmp(ext, "xls") == 0 || strcmp(ext, "xlsx") == 0 || strcmp(ext, "xlsm") == 0)
                {
                    strcpy(command, "\"");strcat(command, xls_program_path);strcat(command, "\" ");
                    strcat(command, "\"");strcat(command, full_attachment_path);strcat(command, "\"");
                    start_process(command);
                }
                else if (strcmp(ext, "doc") == 0 || strcmp(ext, "docx") == 0)
                {
                    strcpy(command, "\"");strcat(command, doc_program_path);strcat(command, "\" ");
                    strcat(command, "\"");strcat(command, full_attachment_path);strcat(command, "\"");
                    start_process(command);
                }
                else if (strcmp(ext, "csv") == 0)
                {
                    strcpy(command, "\"");strcat(command, csv_program_path);strcat(command, "\" ");
                    strcat(command, "\"");strcat(command, full_attachment_path);strcat(command, "\"");
                    start_process(command);
                }
                else if (strcmp(ext, "txt")== 0)
                {
                    strcpy(command, "\"");strcat(command, txt_program_path);strcat(command, "\" ");
                    strcat(command, "\"");strcat(command, full_attachment_path);strcat(command, "\"");
                    start_process(command);
                }
                else if (strcmp(ext, "pdf") == 0 || strcmp(ext, "pdfx") == 0)
                {
                    strcpy(command, "\"");strcat(command, pdf_program_path);strcat(command, "\" ");
                    strcat(command, "\"");strcat(command, full_attachment_path);strcat(command, "\"");
                    start_process(command);
                }
                else if (strcmp(ext, "jpg") == 0 || strcmp(ext, "png") == 0 || strcmp(ext, "gif") == 0)
                {
                    strcat(command, img_program_path);
					strcat(command, " ");
					strcat(command, full_attachment_path);
                    start_process(command);
                }
                else
                {
                    strcpy(command, "\"");strcat(command, misc_program_path);strcat(command, "\" ");
                    strcat(command, "\"");strcat(command, full_attachment_path);strcat(command, "\"");
                    start_process(command);
                }
            }
            char context_id[100] = {0};
            sprintf(context_id,"context##%d",i);

            if (ImGui::BeginPopupContextItem(context_id))
            {
                if (ImGui::Selectable("Copy Attachment Path", (selected_email_index == i),0))
                {
                    char full_attachment_path[260] = {0};

                    if(str_startswith(curr_email.attachments[i],"\\"))
                    {
                        strcpy(full_attachment_path, root_dir);
                        strcat(full_attachment_path, curr_email.attachments[i]);
                    }
                    else
                        strcpy(full_attachment_path,curr_email.attachments[i]);

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

        ImGui::End();

		if (show_settings_window)
		{
			int settings_init_width = 240;
			int settings_init_height = 320;

			ImGui::SetNextWindowPos(ImVec2((window_width - settings_init_width) / 2.0f, (window_height - settings_init_height) / 2.0f), ImGuiCond_FirstUseEver);
			ImGui::SetNextWindowSize(ImVec2(settings_init_width, settings_init_height), ImGuiCond_FirstUseEver);
            ImGui::Begin("Settings", &show_settings_window);

            ImGui::Text("Email Root Directory");
			ImGui::PushItemWidth(-1); ImGui::InputText("##rootDirectory", root_dir, MAX_PATH); ImGui::PopItemWidth();
            ImGui::Separator();
			ImGui::PushItemWidth(-1);
            ImGui::Text("XLS/XLSX/XLSM Program Path");
            ImGui::InputText("##xlsprogrampath",xls_program_path,MAX_PATH);
            ImGui::Text("DOC/DOCX Program Path");
            ImGui::InputText("##docprogrampath",doc_program_path,MAX_PATH);
            ImGui::Text("CSV Program Path");
            ImGui::InputText("##csvprogrampath",csv_program_path,MAX_PATH);
            ImGui::Text("TXT Program Path");
            ImGui::InputText("##txtprogrampath",txt_program_path,MAX_PATH);
            ImGui::Text("PDF Program Path");
            ImGui::InputText("##pdfprogrampath",pdf_program_path,MAX_PATH);
            ImGui::Text("IMG Program Path (PNG,JPG,GIF)");
            ImGui::InputText("##imgprogrampath",img_program_path,MAX_PATH);
            ImGui::Text("Misc Program Path");
            ImGui::InputText("##miscprogrampath",misc_program_path,MAX_PATH);
            ImGui::Text("Export Program Path");
            ImGui::InputText("##exportprogrampath",export_program_path,MAX_PATH);
			ImGui::PopItemWidth();
            ImGui::End();
		}

		//ImGui::ShowDemoWindow(0);

        // Rendering
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui::Render();
        glfwSwapBuffers(window);
    }

    store_config();

    // Cleanup
    ImGui_ImplGlfwGL3_Shutdown();
    glfwTerminate();

    return 0;
}

static char* dt_convert_to_sortable_format(const char* date)
{
    // convert
    // MM/dd/yyyy hh:mm tt
    // to
    // yyyyMMddtthhmm

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

    char* sortable_date = (char*)calloc(14 + 1,sizeof(char));

	strncpy(sortable_date, yyyy, 4);
	strncat(sortable_date, MM, 2);
	strncat(sortable_date, dd, 2);
	strncat(sortable_date, tt, 2);
	strncat(sortable_date, hh, 2);
	strncat(sortable_date, mm, 2);

    return sortable_date;
}

static GLFWimage load_icon(const char* image_path)
{
    int w,h,n;
    unsigned char *imgdata = stbi_load(image_path,&w,&h,&n,4);

	GLFWimage* i = new GLFWimage();

    if(imgdata == NULL)
        return *i;

    i->width = w;
    i->height = h;
    i->pixels = imgdata;

    return *i;
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
		fprintf(fp, "%d;%s", strlen(emails[i].cc), emails[i].cc);
		fprintf(fp, "%d;%s", strlen(emails[i].from), emails[i].from);
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

	fscanf(fp, "%d;", &num_emails);

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
        //if(str_contains(emails[i].to,"CN="))
    }
}

static void load_emails()
{
    clear_email_memory();
	num_emails = 0;
    get_emails(root_dir);

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
            case TO:        items[i].value = e.to; break;
            case CC:        items[i].value = e.cc; break;
            case FROM:      items[i].value = e.from; break;
            case FORWARDED: items[i].value = e.forwarded; break;
            case DATE:      items[i].value = e.date_formatted; break;
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
				case DATE: strncpy(filter, filter_date, strlen(filter_date)); strncpy(compare, emails[i].date, strlen(emails[i].date)); break;
                case TO: strncpy(filter,filter_to,strlen(filter_to)); strncpy(compare,emails[i].to,strlen(emails[i].to)); break;
				case FROM: strncpy(filter, filter_from, strlen(filter_from)); strncpy(compare, emails[i].from, strlen(emails[i].from)); break;
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

static void store_config()
{
    FILE *fp = fopen(config_file_path,"w");
	
	if (fp == NULL)
		return;
    
	fprintf(fp,"root_dir: %s\n",root_dir);
	fprintf(fp,"xls_program_path: %s\n",xls_program_path);
	fprintf(fp,"doc_program_path: %s\n",doc_program_path);
	fprintf(fp,"csv_program_path: %s\n",csv_program_path);
	fprintf(fp,"txt_program_path: %s\n",txt_program_path);
	fprintf(fp,"pdf_program_path: %s\n",pdf_program_path);
	fprintf(fp,"img_program_path: %s\n",img_program_path);
	fprintf(fp,"misc_program_path: %s\n",misc_program_path);
	fprintf(fp,"export_program_path: %s\n",export_program_path);
    fprintf(fp,"emails_window_width_pct: %f\n",emails_window_width_pct);
	fprintf(fp, "selected_style_index: %d\n", selected_style_index);
	fprintf(fp, "selected_font_index: %d\n", selected_font_index);

    fclose(fp);
}

static void load_config()
{
    FILE *fp = fopen(config_file_path,"r");

	if (fp == NULL)
		return;

    fscanf(fp,"root_dir: %s\n",root_dir);
	fscanf(fp,"xls_program_path: %260[^\n]\n",xls_program_path);
	fscanf(fp,"doc_program_path: %260[^\n]\n",doc_program_path);
	fscanf(fp,"csv_program_path: %260[^\n]\n",csv_program_path);
	fscanf(fp,"txt_program_path: %260[^\n]\n",txt_program_path);
	fscanf(fp,"pdf_program_path: %260[^\n]\n",pdf_program_path);
	fscanf(fp,"img_program_path: %260[^\n]\n",img_program_path);
	fscanf(fp,"misc_program_path: %260[^\n]\n",misc_program_path);
	fscanf(fp,"export_program_path: %260[^\n]\n",export_program_path);
    fscanf(fp,"emails_window_width_pct: %f\n",&emails_window_width_pct);
	fscanf(fp, "selected_style_index: %d\n", &selected_style_index);
	fscanf(fp, "selected_font_index: %d\n", &selected_font_index);
    fclose(fp);
}

static void clear_email_memory()
{
    for(int i = 0; i < num_emails; ++i)
    {
        if(emails[i].file_path != NULL) {free(emails[i].file_path); emails[i].file_path = NULL;}
        if(emails[i].to != NULL)        {free(emails[i].to);emails[i].to = NULL;}
        if(emails[i].cc != NULL)        {free(emails[i].cc);emails[i].cc = NULL;}
        if(emails[i].from != NULL)      {free(emails[i].from);emails[i].from = NULL;}
        if(emails[i].forwarded != NULL) {free(emails[i].forwarded);emails[i].forwarded = NULL;}
        if(emails[i].date != NULL)      {free(emails[i].date);emails[i].date = NULL;}
        if(emails[i].date_formatted != NULL) { free(emails[i].date_formatted);emails[i].date_formatted = NULL;}
        if(emails[i].subject != NULL)   {free(emails[i].subject);emails[i].subject = NULL;}
        if(emails[i].body != NULL)      {free(emails[i].body);emails[i].body = NULL;}

        for(int j = 0; j < 100; ++j)
        {
            if (emails[i].attachments[j] != NULL) {free(emails[i].attachments[j]);emails[i].attachments[j] = NULL;}
            if (emails[i].attachments_ext[j] != NULL) {free(emails[i].attachments_ext[j]);emails[i].attachments_ext[j] = NULL;}
        }

        emails[i].attachment_count = 0;
    }

	if (filtered_indicies != NULL) {free(filtered_indicies);filtered_indicies = NULL;}
	if (sorted_indicies   != NULL) {free(sorted_indicies);sorted_indicies = NULL;}

}

static void export_emails()
{
    char command[260] = {0};

    strcpy(command,export_program_path);
    strcat(command," ");
    strcat(command,root_dir);

    start_process(command,true);
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
        strcpy(full_path,directory_path);
        strcat(full_path,"\\");
        strcat(full_path,dp->d_name);

        if(str_startswith(dp->d_name,"email_"))
        {
            emails[num_emails].file_path = (char*)calloc(strlen(full_path)+1,sizeof(char));
            strcpy(emails[num_emails].file_path,full_path);
            //parse_email(full_path, num_emails);
			++num_emails;
        }

        get_emails(full_path);
    }

    closedir(dir);
}

static void display_main_menu()
{
	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("Quit", "Alt+F4"))
                glfwSetWindowShouldClose(window,1); 
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Settings"))
		{
            if(ImGui::MenuItem("Edit Settings..."))
                show_settings_window = !show_settings_window;
            ImGui::Separator();
			ImGui::DragFloat("Emails Width \%", &emails_window_width_pct, 0.01f);
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
            if (ImGui::MenuItem("Export Emails From Lotus..."))
                export_emails();
            
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
		ImGui::EndMainMenuBar();
	}
}

static bool show_style_selector(const char* label)
{
    if (ImGui::Combo(label, &selected_style_index, "Classic\0Dark\0Light\0Monochrome\0Ecstacy\0Minimal"))
    {
		switch (selected_style_index)
        {
        case 0: ImGui::StyleColorsClassic(); header_color = ImVec4(0.4f,0.2f,0.8f,1.0f);break;
        case 1: ImGui::StyleColorsDark();    header_color = ImVec4(0.4f,0.5f,0.7f,1.0f);break;
        case 2: ImGui::StyleColorsLight();   header_color = ImVec4(0.2f,0.4f,0.9f,1.0f);break;
        case 3: load_style_monochrome();     header_color = ImVec4(0.07f,0.34f,0.34f,1.0f);break;
        case 4: load_style_ecstacy();        header_color = ImVec4(1.0f,1.0f,1.0f,1.0f);break;
        case 5: load_style_minimal();        header_color = ImVec4(0.4f,0.4f,0.4f,1.0f);break;
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

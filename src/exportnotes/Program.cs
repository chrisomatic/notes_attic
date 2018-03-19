using System;
using System.Collections.Generic;
using System.IO;
using System.Xml;
using Domino;

namespace exportnotes
{
    class Program
    {
        public struct NotesEmail
        {
            public string   id;
            public string   to;
            public string   from;
            public string   net_to;
            public string   net_from;
            public string   copied;
            public string   forwarded;
            public string   subject;
            public string   body;
            public string   delivered_date;
            public string   creation_time;
            public string   original_mod_time;
            public string   posted_date;
            public int      attachment_count;
            public List<string> attachment_paths;
        };

        static NotesSessionClass session;
        static NotesDatabase     db;
        static NotesView         view;
        static int               curr_document;
        static StreamWriter      sw;
        static NotesDXLExporter  dxl;
        static NotesEmail email;
        static string     server;
        static string     pass;
        static string     specified_nsf;
        static string     export_dir;
        static bool       quiet;
        
        static int orig_row;
        static int orig_col;

        static void display_help()
        {
            wrl("Export Notes V1.0 By Chris Rose [czr]");
            wrl("Exports all of your Lotus Notes Emails and Attachments to a specified location. Useful for backing up emails if you are running out of space, or just want a convenient way to look back at them.");
            wrl();
            wrl("Usage:");
            wrl(" exportnotes <export_directory> [-s server -p password -q -?]");
            wrl();
            wrl("Options:");
            wrl();
            wrl(" -s: Specify the mail server. This can be an IP address or Domain Name. Default is Email01.");
            wrl(" -p: Specify password. If this option is not specified, you'll be prompted for a password");
            wrl(" -q: Quiet. Suppress all unnecessary output from program.");
            wrl(" -f: Specify the .nsf file that will override the default assumed name (no need to include extension). e.g -f john_doe2");
            wrl(" -?: Show this help display.");
            wrl();
            wrl("Examples:");
            wrl(" exportnotes C:\\email_archive");
            wrl(" exportnotes C:\\email_archive -s Email02 -p MyPass");
            wrl();
        }

        static int Main(string[] args)
        {

#if (DEBUG)
            //args = new string[] { };
            args = new string[] { "C:\\email_archive_test"};
#endif
            
            // initialization
            Console.TreatControlCAsInput = true;

            export_dir = "";
            server = "Email01";
            pass   = "";
            specified_nsf = "";
            quiet  = false;
            curr_document = 0;

            // handle commandline args
            if (args.Length == 0)
            {
                display_help();
#if (DEBUG)
                wr("\nPress any key to exit...");
                System.Console.ReadKey();
#endif
                return 0;
            }

            for(int i = 0; i < args.Length;++i)
            { 
                if (args[i][0] == '-')
                {
                    if (args[i].Length < 2)
                    {
                        wrl_error("\nPlease specify an option instead of an empty dash -");
                        return 1;
                    }

                    switch (args[i][1])
                    {
                        case '?':
                            display_help();
                            return 0;
                        case 's':
                            if (args.Length > i + 1)
                            {
                                server = args[i + 1];
                                ++i;
                            }
                            break;
                        case 'f':
                            if (args.Length > i + 1)
                            {
                                specified_nsf = args[i + 1];
                                ++i;
                            }
                            break;
                        case 'p':
                            if (args.Length > i + 1)
                            {
                                pass = args[i + 1];
                                ++i;
                            }
                            break;
                        case 'q':
                            quiet = true;
                            break;
                    }
                }
                else
                {
                    if(export_dir == "")
                        export_dir = args[i];
                }
            }

            // validation
            if(export_dir == "")
            {
                wrl_error("\nPlease specify an export directory. Run \"exportnotes -?\" for help");
                return 1;
            }

            session = new NotesSessionClass();

            // get password from user if it wasn't specified
            if(pass == "")
            {
                // prompt for password
                bool pass_success = false;

                ConsoleKeyInfo ki;
                char c;
                string pass_attempt = "";

                while (!pass_success)
                {
                    wr("password:");

                    pass_attempt = "";
                    for (;;)
                    {
                        ki = Console.ReadKey(true);

                        if (ki.Key == ConsoleKey.Escape || (ki.Modifiers == ConsoleModifiers.Control && ki.Key == ConsoleKey.C))
                        {
                            wr("\n");
                            wrl("Exiting...");
                            return 1;
                        }

                        c = ki.KeyChar;
                        if (c == '\r')
                            break;
                        pass_attempt += c;
                    };

                    wr("\n");

                    try
                    {
                        session.Initialize(pass_attempt);
                        wrl("Connected.");
                        pass_success = true;
                    }
                    catch
                    {
                        wrl("Failed to connect.");
                    }
                }
            }
            else
            {
                try
                {
                    session.Initialize(pass);
                    wrl("Connected.");
                }
                catch
                {
                    wrl("Failed to connect.");
                    return 1;
                }
            }

            dxl = session.CreateDXLExporter();

            string nsf_file_name = "";

            if (specified_nsf == "")
                nsf_file_name = get_nsf_file_name();
            else
                nsf_file_name = specified_nsf;

            db = session.GetDatabase(server,nsf_file_name, false);

            if (db == null)
            {
                wrl("Failed to open database " + nsf_file_name + " on server " + server);
                return 1;
            }
            
            archive();

            wr("\nPress any key to exit...");
            System.Console.ReadKey();

            return 0;
        }

        static void archive()
        {
            // create folders if needed
            if (!Directory.Exists(export_dir))
                Directory.CreateDirectory(export_dir);

            if (!Directory.Exists(export_dir + "\\emails"))
                Directory.CreateDirectory(export_dir + "\\emails");

            if (!Directory.Exists(export_dir + "\\attachments"))
                Directory.CreateDirectory(export_dir + "\\attachments");

            view = db.GetView("$All");

            if(view == null)
                view = db.GetView("$Inbox");

            if(view == null)
            {
                wrl_error("Failed to get viable view; Tried $All and $Inbox.");
                return;
            }

            NotesDocument document = view.GetFirstDocument();

            if(!quiet) wrl("Getting Email Count...");

            int num_documents = get_num_documents();
            
            // open up error log
            StreamWriter sw_errors = new StreamWriter(export_dir + "\\error_log.txt");

            int num_errored = 0;
            curr_document = 0;

            if (!quiet) wrl("Exporting Emails... (ESC or CTRL-C to stop)");

            orig_row = Console.CursorTop;
            orig_col = Console.CursorLeft;
            
            int max_progress = 50;
            int curr_progress = 0;
            float trigger_progress = 0.0F;
            float trigger_point = num_documents / (float)max_progress;

            int length_of_num_documents = num_documents.ToString().Length;

            string progress_format_string = "{0:";
            for(int f = 0; f < length_of_num_documents; ++f)
                progress_format_string += "0";
            progress_format_string += "}";

            if(!quiet)
            {
                write_at("[",0,1);
                write_at("]",max_progress+1,1);
            }

            int document_count = 0;

            

            while (document != null)
            {
                // Check to see if exit key was pressed
                if (Console.KeyAvailable)
                {
                    ConsoleKeyInfo ki = Console.ReadKey(true);

                    if (ki.Key == ConsoleKey.Escape || (ki.Modifiers == ConsoleModifiers.Control && ki.Key == ConsoleKey.C))
                    {
                        wrl("Exiting...");
                        if(sw != null)
                            sw.Close();
                        if (sw_errors != null)
                            sw_errors.Close();
                        return;
                    }
                }

                // reset email object
                email.attachment_count = 0;
                email.attachment_paths = new List<string>();
                email.to = null;
                email.from = null;
                email.net_to = null;
                email.net_from = null;
                email.copied = null;
                email.forwarded = null;
                email.subject = null;
                email.body = null;
                email.delivered_date = null;
                email.creation_time = null;
                email.original_mod_time = null;
                email.posted_date = null;

                object documentItems = document.Items;

                if(document.Items == null)
                {
                    // get column values to write to error log ...
                    
                    List<object> columnList = new List<object>();

                    object col_vals = document.ColumnValues;

                    foreach (object o in (object[])col_vals)
                    {
                        columnList.Add(o);
                    }

                    string from = (string)columnList[3];
                    string subject = (string)columnList[6];
                    string date = ((DateTime)columnList[7]).ToLongDateString();

                    sw_errors.WriteLine(string.Format("{0}: No Items array found on document",curr_document));
                    sw_errors.WriteLine(string.Format("from: {0}", from));
                    sw_errors.WriteLine(string.Format("subject: {0}", subject));
                    sw_errors.WriteLine(string.Format("date: {0}", date));
                    sw_errors.WriteLine("--------------------------------------------------------------------------------------------"); 
                    // get next document
                    document = view.GetNextDocument(document);
                    ++num_errored;
                    ++curr_document;
                    continue;
                }

                Array items = (System.Array)documentItems;

                for (int i = 0; i < items.Length; ++i)
                {
                    NotesItem notesItem = (Domino.NotesItem)items.GetValue(i);

                    switch (notesItem.Name)
                    {
                        case "$MessageID": email.id = notesItem.Text; break;
                        case "Subject": email.subject = notesItem.Text; break;
                        case "InetSendTo": email.net_to = notesItem.Text; break;
                        case "INetFrom": email.net_from = notesItem.Text; break;
                        case "InetCopyTo": email.copied = notesItem.Text; break;
                        case "ForwardedFrom": email.forwarded = notesItem.Text; break;
                        case "Body":
                            {
                                NotesItem body_item = document.GetFirstItem("Body");

                                NotesRichTextItem rt_item = null;
                                if (body_item is NotesRichTextItem)
                                {
                                    rt_item = (NotesRichTextItem)body_item;

                                    // try to get embedded img tags and save them as attachment

                                    string dxl_output = dxl.Export(document);

                                    if (dxl_output.Contains("<gif>"))
                                    {
                                        // remove !DOCTYPE line

                                        int doctype_start = dxl_output.IndexOf("<!DOCTYPE");
                                        int doctype_end = dxl_output.IndexOf(".dtd'>");

                                        dxl_output = dxl_output.Remove(doctype_start, Math.Max(0,doctype_end - doctype_start + 6));

                                        //string[] lines = dxl_output.Split('\n');
                                        //string dxl_output_modified = "";

                                        /*
                                        for (int l = 0; l < lines.Length; ++l)
                                        {
                                            if (lines[l].Contains("!DOCTYPE"))
                                                continue;

                                            dxl_output_modified += lines[l];

                                            if (l < lines.Length - 1)
                                                dxl_output_modified += '\n';
                                        }
                                        */
                                        XmlDocument doc = new XmlDocument();
                                        try
                                        {
                                            doc.LoadXml(dxl_output);

                                            XmlNodeList elemList = doc.GetElementsByTagName("gif");

                                            for (int k = 0; k < elemList.Count; k++)
                                            {
                                                string actual_data = elemList[k].InnerXml.Replace("\n", "");
                                                byte[] data = Convert.FromBase64String(actual_data);
                                                
                                                string attachment_name = string.Format("{0}.gif", actual_data.Substring(0, 24));
                                                string file_path = string.Format(@"{0}\attachments\{1}", export_dir, attachment_name);
                                                File.WriteAllBytes(file_path, data);

                                                email.attachment_paths.Add(String.Format(@"\attachments\{0}", attachment_name));
                                                ++email.attachment_count;
                                            }
                                        }
                                        catch
                                        {
                                            // couldn't load xml, shame.
                                        }
                                    }

                                    if (rt_item.EmbeddedObjects != null)
                                    {
                                        foreach (object o in (Object[])rt_item.EmbeddedObjects)
                                        {
                                            NotesEmbeddedObject att = (NotesEmbeddedObject)o;

                                            if (att.type == EMBED_TYPE.EMBED_ATTACHMENT)
                                            {

                                                string file_path = string.Format(@"{0}\attachments\{1}", export_dir, att.Name);
                                                string attachment_name_without_ext = System.IO.Path.GetFileNameWithoutExtension(att.Name);
                                                string attachment_ext = System.IO.Path.GetExtension(att.Name);

                                                string attachment_name = att.Name;

                                                int append_number = 0;

                                                while (System.IO.File.Exists(file_path))
                                                {
                                                    long length = new System.IO.FileInfo(file_path).Length;

                                                    if (length == att.FileSize)
                                                        break;

                                                    attachment_name = String.Concat(attachment_name_without_ext, "_", append_number, attachment_ext);
                                                    file_path = String.Format(@"{0}\attachments\{1}", export_dir, attachment_name);

                                                    append_number++;
                                                }

                                                att.ExtractFile(file_path);

                                                email.attachment_paths.Add(String.Format(@"\attachments\{0}", attachment_name));
                                                ++email.attachment_count;
                                            }
                                        }
                                    }
                                }
                                email.body = notesItem.Text;
                            } break;
                        case "DeliveredDate": email.delivered_date = notesItem.Text; break;
                        case "From": email.from = notesItem.Text; break;
                        case "SendTo": email.to = notesItem.Text; break;
                        case "ExCreationTime": email.creation_time = notesItem.Text; break;
                        case "OriginalModTime": email.original_mod_time = notesItem.Text; break;
                        case "PostedDate": email.posted_date = notesItem.Text; break;
                    }
                }

                // write out email
                write_out_email_to_file();

                document = view.GetNextDocument(document);
                ++curr_document;

                if (!quiet && curr_document <= num_documents) write_at(String.Format(progress_format_string + "/{1}", curr_document, num_documents), 0, 0);

                ++trigger_progress;
                while(trigger_progress >= trigger_point)
                {
                    ++curr_progress;
                    trigger_progress -= trigger_point;

                    if(!quiet) write_at("=",curr_progress, 1);
                }

                ++document_count;
            }

            if (!quiet) write_at("=", curr_progress, 1);

            if (!quiet)
            {
                Console.CursorTop += 2;
                Console.CursorLeft = 0;
            }
            if (!quiet) wrl("Emails successfully exported to " + export_dir);
            if (!quiet) wrl("Errored Emails: " + num_errored.ToString());
            if (!quiet && num_errored > 0) wrl("For error details look at this file: error_log.txt");

            sw_errors.Close();
        }

        static void write_out_email_to_file()
        {
            string email_sender;

            if(email.net_from == null)
                email_sender = email.from;
            else
                email_sender = email.net_from;

            string email_sender_formatted = format_sender(email_sender);
            email_sender_formatted = replace_unwanted_chars(email_sender_formatted);

            string sender_dir = String.Format("{0}\\emails\\{1}",export_dir,email_sender_formatted);

            // create sender dir
            if (!Directory.Exists(sender_dir))
                Directory.CreateDirectory(sender_dir);

            // create year dir if it needs created
            string year = "";
            DateTime dt = new DateTime();

            // MM/dd/yyyy HH:mm:ss tt 
            if(email.delivered_date != null)
                dt = DateTime.Parse(email.delivered_date,System.Globalization.CultureInfo.CurrentCulture,System.Globalization.DateTimeStyles.AssumeLocal);
            else if(email.posted_date != null)
                dt = DateTime.Parse(email.posted_date,System.Globalization.CultureInfo.CurrentCulture,System.Globalization.DateTimeStyles.AssumeLocal);
            else if(email.creation_time != null)
                dt = DateTime.Parse(email.creation_time,System.Globalization.CultureInfo.CurrentCulture,System.Globalization.DateTimeStyles.AssumeLocal);
            else if(email.original_mod_time != null)
                dt = DateTime.Parse(email.original_mod_time,System.Globalization.CultureInfo.CurrentCulture,System.Globalization.DateTimeStyles.AssumeLocal);

            if(dt.Year > 0)
                year = dt.Year.ToString();

            string year_dir = string.Format(@"{0}\{1}",sender_dir, year);

            if(!Directory.Exists(year_dir))
                Directory.CreateDirectory(year_dir);

            // Create file path
            int email_counter = 0;
            string file_path = String.Format(@"{0}\email_{1:yyyyMMddHHmm}.txt", year_dir, dt);

            if (email.id == null)
                email.id = "";

            // check to see if file exists has a different id or not
            if (is_file_unique(file_path))
            {
                // get unique file path
                while (File.Exists(file_path))
                {
                    file_path = String.Format(@"{0}\email_{1:yyyyMMddHHmm}_{2}.txt", year_dir, dt, email_counter);
                    if (!is_file_unique(file_path))
                        return; // file is not unique

                    email_counter++;
                } 
                
                // write file
                sw = new StreamWriter(file_path);

                sw.WriteLine("@" + email.id);
                sw.WriteLine("To: " + email.to);
                sw.WriteLine("CC: " + email.copied);
                sw.WriteLine("From: " + email.from);
                sw.WriteLine("Forwarded From: " + email.forwarded);
                sw.WriteLine("Date: " + String.Format("{0:MM/dd/yyyy hh:mm tt}", dt));
                sw.WriteLine("Subject: " + email.subject);
                sw.WriteLine("Attachment_Count: " + email.attachment_count);

                for (int a = 0; a < email.attachment_count; ++a)
                {
                    sw.WriteLine(string.Format("Attachment_{0}: {1}", a, email.attachment_paths[a]));
                }
                sw.WriteLine("Body:\r\n");
                sw.WriteLine(email.body);

                sw.Close();
            }
        }

        static bool is_file_unique(string file_path)
        {
            if (File.Exists(file_path))
            {
                StreamReader sr = new StreamReader(file_path);
                string line = sr.ReadLine();

                string existing_id = "";
                if (line.StartsWith("@"))
                    existing_id = line.Substring(1, line.Length - 1);

                if (existing_id == email.id)
                    return false;
            }

            return true;
        }

        static int get_num_documents()
        {
            NotesDocument document = view.GetFirstDocument();

            int count = 0;
            while (document != null)
            {
                count++;
                document = view.GetNextDocument(document);
            }

            return count;
        }

        static string get_nsf_file_name()
        {
            string user_name = session.CommonUserName;

            string[] string_components = user_name.Split(' ');

            if(string_components.Length == 1)
                user_name = string_components[0];
            else if(string_components.Length == 2)
                user_name = String.Format("{0}{1}",string_components[0][0],string_components[1]);
            else if(string_components.Length == 3)
                user_name = String.Format("{0}{1}",string_components[0][0],string_components[2]);
            else
            {
                user_name = String.Format("{0}{1}",string_components[0][0],string_components[string_components.Length -1]);
            }

            user_name = user_name.ToLower();

            if(user_name.Length > 8)
                user_name = user_name.Substring(0,8);

            return string.Format("mail\\{0}.nsf",user_name);

        }

        static string replace_unwanted_chars(string s)
        {
            string s_formatted = "";

            for(int j = 0; j < s.Length; ++j)
            {
                bool valid_char = false;

                if ((s[j] >= 48 && s[j] <= 57) || (s[j] >= 65 && s[j] <= 90) || (s[j] >= 97 && s[j] <= 122)) // is valid
                {
                    valid_char = true;
                    s_formatted += s[j];
                }

                if(!valid_char)
                    s_formatted += '_';
            }

            return s_formatted;

        }

        static string format_sender(string s)
        {
            // we want: email_name_with_underscores
            string formatted_sender = "";

            if(s.Contains("@"))
            {

                string new_string = "";

                if(s.Contains("<"))
                {
                    // "Name" <email@whatever.com>
                    // <email@whatever.com>

                    int index = 0;

                    while(s[index] != '<')
                        index++;

                    for(int i = index + 1; i < s.Length; ++i)
                    {
                        if(s[i] == '>')
                            break;

                        new_string += s[i];
                    }

                    s = new_string;
                }

                // email@whatever.com
                string[] email_components = s.Split('@');

                formatted_sender = email_components[0].ToLower();

            }
            else
            {
                // CN=Name With Spaces/O=ONLI
                // CN=Name With Spaces

                string[] string_components = s.Split('=');

                string new_string = "";

                if(string_components.Length >= 2)
                {
                    new_string = string_components[1];

                    string_components = new_string.Split('/');
                    new_string = string_components[0];

                    string_components = new_string.Split(' ');

                    if(string_components.Length == 1)
                    {
                        new_string = string_components[0];
                    }
                    else if(string_components.Length == 2)
                    {
                        new_string = String.Format("{0}_{1}",string_components[0],string_components[1]);
                    }
                    else if(string_components.Length == 3)
                    {
                        new_string = String.Format("{0}_{1}",string_components[0],string_components[2]);
                    }
                    else
                    {
                        // more than 3 names! weird.
                        new_string = String.Format("{0}_{1}",string_components[0],string_components[string_components.Length -1]);
                    }
                }

                formatted_sender = new_string.ToLower();
            }

            return formatted_sender;
        }

        static void write_at(string s, int x, int y)
        {
            try
            {
                Console.SetCursorPosition(orig_col+x, orig_row+y);
                wr(s);
            }
            catch (ArgumentOutOfRangeException e)
            {
                Console.Clear();
                wrl(e.Message);
            }
        }
        static void wr(string _s) { System.Console.Write(_s); }
        static void wrl(string _s) { System.Console.WriteLine(_s); }
        static void wrl() { System.Console.WriteLine(); }
        static void wrl_error(string _s)
        {
            System.ConsoleColor prevColor = System.Console.ForegroundColor;
            System.Console.ForegroundColor = System.ConsoleColor.Red;
            System.Console.WriteLine(_s);
            System.Console.ForegroundColor = prevColor;
        }
    }
}

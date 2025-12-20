/**
 * @file ui.c
 * @brief GTK+ 기반 그래픽 사용자 인터페이스(GUI) 구현
 * @details 로그인 창, 채팅 창(상단 이름 표시 추가), CSS 스타일링, 이벤트 핸들러
 */

#include "ui.h"
#include "network.h"

//  UI 위젯 전역 변수
static GtkWidget *main_window;      // 메인 채팅 창
static GtkWidget *login_window;     // 로그인 창
static GtkWidget *chat_box;         // 말풍선이 쌓이는 컨테이너
static GtkWidget *scrolled_window;  // 스크롤 영역
static GtkWidget *entry_msg;        // 메시지 입력창
static GtkWidget *entry_name;       // 로그인: 이름 입력
static GtkWidget *entry_ip;         // 로그인: IP 입력
static GtkWidget *entry_port;       // 로그인: 포트 입력

/**
 * @brief 어플리케이션의 테마(CSS)를 로드하고 적용합니다.
 */
void init_ui_theme() {
    GtkCssProvider *provider = gtk_css_provider_new();
    const char *css_data = 
        "* { font-family: 'NanumGothic', 'Malgun Gothic', Sans; }"
        "window { background-color: #b2c7d9; }"
        "entry { font-size: 14px; border-radius: 5px; min-height: 30px; }"
        "button { font-weight: bold; background-color: #f0f0f0; border-radius: 5px; }"
        "button:hover { background-color: #e0e0e0; }"
        ".my-bubble { background-color: #fef01b; color: black; border-radius: 5px; padding: 10px; margin: 5px; box-shadow: 1px 1px 1px gray; }"
        ".other-bubble { background-color: #ffffff; color: black; border-radius: 5px; padding: 10px; margin: 5px; box-shadow: 1px 1px 1px gray; }"
        ".system-msg { color: #555555; font-size: 11px; margin: 5px; padding: 4px; background-color: rgba(255,255,255,0.5); border-radius: 10px; }";

    GError *error = NULL;
    gtk_css_provider_load_from_data(provider, css_data, -1, &error);
    
    if (error) {
        fprintf(stderr, "CSS Load Error: %s\n", error->message);
        g_error_free(error);
    } else {
        gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
                                                  GTK_STYLE_PROVIDER(provider),
                                                  GTK_STYLE_PROVIDER_PRIORITY_USER);
    }
}

void copy_file(const char *src, const char *dest) {
    FILE *source = fopen(src, "rb");
    FILE *target = fopen(dest, "wb");
    if (!source || !target) return;
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), source)) > 0) fwrite(buf, 1, n, target);
    fclose(source); fclose(target);
}

void on_download_clicked(GtkWidget *widget, gpointer data) {
    char *filename = (char *)data;
    char temp_name[300];
    sprintf(temp_name, "temp_%s", filename);

    GtkWidget *dialog = gtk_file_chooser_dialog_new("파일 저장", GTK_WINDOW(main_window), GTK_FILE_CHOOSER_ACTION_SAVE, "취소", GTK_RESPONSE_CANCEL, "저장", GTK_RESPONSE_ACCEPT, NULL);
    gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog), filename);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char *dest_path = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        copy_file(temp_name, dest_path);
        GtkWidget *msg = gtk_message_dialog_new(GTK_WINDOW(main_window), GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_OK, "파일 저장이 완료되었습니다.\n경로: %s", dest_path);
        gtk_dialog_run(GTK_DIALOG(msg));
        gtk_widget_destroy(msg);
        g_free(dest_path);
    }
    gtk_widget_destroy(dialog);
}

gboolean ui_update_callback(gpointer user_data) {
    ChatData *data = (ChatData *)user_data;
    GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    GtkWidget *content_widget;

    if (data->is_file_btn) {
        char btn_label[300];
        sprintf(btn_label, "%s (다운로드)", data->real_filename);
        content_widget = gtk_button_new_with_label(btn_label);
        g_signal_connect(content_widget, "clicked", G_CALLBACK(on_download_clicked), g_strdup(data->real_filename));
    } else {
        content_widget = gtk_label_new(data->msg);
        gtk_label_set_line_wrap(GTK_LABEL(content_widget), TRUE);
        gtk_label_set_max_width_chars(GTK_LABEL(content_widget), 30);
        gtk_label_set_xalign(GTK_LABEL(content_widget), 0.0);
    }

    if (strcmp(data->name, PROTOCOL_SYSTEM) == 0) {
        GtkWidget *center = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
        gtk_box_pack_start(GTK_BOX(center), content_widget, TRUE, FALSE, 0);
        if (!data->is_file_btn) gtk_style_context_add_class(gtk_widget_get_style_context(content_widget), "system-msg");
        gtk_box_pack_start(GTK_BOX(chat_box), center, FALSE, FALSE, 5);
    } 
    else if (data->is_mine) {
        if (!data->is_file_btn) gtk_style_context_add_class(gtk_widget_get_style_context(content_widget), "my-bubble");
        gtk_box_pack_end(GTK_BOX(row), content_widget, FALSE, FALSE, 0);
        GtkWidget *spacer = gtk_label_new("");
        gtk_box_pack_start(GTK_BOX(row), spacer, TRUE, TRUE, 0);
        gtk_box_pack_start(GTK_BOX(chat_box), row, FALSE, FALSE, 2);
    } 
    else {
        GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
        GtkWidget *name_lbl = gtk_label_new(data->name);
        gtk_widget_set_halign(name_lbl, GTK_ALIGN_START);
        gtk_style_context_add_class(gtk_widget_get_style_context(name_lbl), "system-msg");
        
        if (!data->is_file_btn) gtk_style_context_add_class(gtk_widget_get_style_context(content_widget), "other-bubble");
        gtk_box_pack_start(GTK_BOX(vbox), name_lbl, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(vbox), content_widget, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(row), vbox, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(chat_box), row, FALSE, FALSE, 2);
    }

    gtk_widget_show_all(chat_box);
    GtkAdjustment *adj = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(scrolled_window));
    gtk_adjustment_set_value(adj, gtk_adjustment_get_upper(adj));

    if(data->name) free(data->name);
    if(data->msg) free(data->msg);
    if(data->real_filename) free(data->real_filename);
    free(data);
    return FALSE;
}

void add_system_msg(const char *msg) {
    ChatData *d = malloc(sizeof(ChatData));
    d->name = strdup(PROTOCOL_SYSTEM); d->msg = strdup(msg);
    d->is_mine = 0; d->is_file_btn = 0; d->real_filename = NULL;
    g_idle_add(ui_update_callback, d);
}

void add_chat_bubble(const char *name, const char *msg, int is_mine) {
    ChatData *d = malloc(sizeof(ChatData));
    d->name = strdup(name); d->msg = strdup(msg);
    d->is_mine = is_mine; d->is_file_btn = 0; d->real_filename = NULL;
    g_idle_add(ui_update_callback, d);
}

void add_file_download_btn(const char *filename) {
    ChatData *d = malloc(sizeof(ChatData));
    d->name = strdup(PROTOCOL_SYSTEM); d->msg = NULL;
    d->is_mine = 0; d->is_file_btn = 1;
    d->real_filename = strdup(filename);
    g_idle_add(ui_update_callback, d);
}

void on_send_clicked(GtkWidget *widget, gpointer data) {
    const char *msg = gtk_entry_get_text(GTK_ENTRY(entry_msg));
    if (strlen(msg) > 0) {
        send_text_message(msg);
        add_chat_bubble(my_name, msg, 1);
        gtk_entry_set_text(GTK_ENTRY(entry_msg), "");
    }
}

void on_file_send_clicked(GtkWidget *widget, gpointer data) {
    GtkWidget *dialog = gtk_file_chooser_dialog_new("전송할 파일 선택", GTK_WINDOW(main_window), GTK_FILE_CHOOSER_ACTION_OPEN, "취소", GTK_RESPONSE_CANCEL, "전송", GTK_RESPONSE_ACCEPT, NULL);
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        char temp[300]; sprintf(temp, "파일 전송 시작: %s", g_path_get_basename(filename));
        add_system_msg(temp);
        pthread_t t;
        pthread_create(&t, NULL, send_file_thread, filename);
    }
    gtk_widget_destroy(dialog);
}

//  채팅 화면 생성
void create_chat_window() {
    main_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(main_window), "오픈소스 톡");
    gtk_window_set_default_size(GTK_WINDOW(main_window), 420, 650);
    g_signal_connect(main_window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(main_window), vbox);

    // 상단 접속자 이름 표시
    char title_buf[256];
    sprintf(title_buf, "<span size='large' weight='bold' color='#333'> 접속자: %s</span>", my_name);
    
    GtkWidget *lbl_header = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(lbl_header), title_buf);
    
    // 라벨 여백 설정 (위 10px, 아래 5px)
    gtk_widget_set_margin_top(lbl_header, 10);
    gtk_widget_set_margin_bottom(lbl_header, 5);
    
    // 박스 맨 위에 추가
    gtk_box_pack_start(GTK_BOX(vbox), lbl_header, FALSE, FALSE, 0);
    // ---------------------------------------------------------

    scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_box_pack_start(GTK_BOX(vbox), scrolled_window, TRUE, TRUE, 0);

    chat_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(scrolled_window), chat_box);

    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_container_set_border_width(GTK_CONTAINER(hbox), 10);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

    GtkWidget *btn_f = gtk_button_new_with_label("+");
    gtk_widget_set_tooltip_text(btn_f, "파일 전송");
    g_signal_connect(btn_f, "clicked", G_CALLBACK(on_file_send_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(hbox), btn_f, FALSE, FALSE, 0);

    entry_msg = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry_msg), "메시지 입력...");
    g_signal_connect(entry_msg, "activate", G_CALLBACK(on_send_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(hbox), entry_msg, TRUE, TRUE, 0);

    GtkWidget *btn_s = gtk_button_new_with_label("전송");
    g_signal_connect(btn_s, "clicked", G_CALLBACK(on_send_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(hbox), btn_s, FALSE, FALSE, 0);

    gtk_widget_show_all(main_window);
}

void on_connect_clicked(GtkWidget *widget, gpointer data) {
    const char *name = gtk_entry_get_text(GTK_ENTRY(entry_name));
    const char *ip = gtk_entry_get_text(GTK_ENTRY(entry_ip));
    const char *port_str = gtk_entry_get_text(GTK_ENTRY(entry_port));

    if(strlen(name) == 0 || strlen(ip) == 0 || strlen(port_str) == 0) return;
    strcpy(my_name, name);

    if (connect_to_server(ip, atoi(port_str)) == 0) {
        gtk_widget_hide(login_window); // 창 숨기기
        create_chat_window();
        pthread_t t_id;
        pthread_create(&t_id, NULL, recv_msg_thread, NULL);
    } else {
        GtkWidget *msg = gtk_message_dialog_new(GTK_WINDOW(login_window), GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, "서버 연결 실패!\nIP와 포트를 확인하세요.");
        gtk_dialog_run(GTK_DIALOG(msg));
        gtk_widget_destroy(msg);
    }
}

void activate_ui(int argc, char *argv[]) {
    init_ui_theme();

    login_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(login_window), "로그인");
    gtk_window_set_default_size(GTK_WINDOW(login_window), 300, 250);
    gtk_window_set_position(GTK_WINDOW(login_window), GTK_WIN_POS_CENTER);
    g_signal_connect(login_window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 20);
    gtk_container_add(GTK_CONTAINER(login_window), vbox);

    GtkWidget *lbl_title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(lbl_title), "<span size='xx-large' weight='bold'>OpenTalk</span>");
    gtk_box_pack_start(GTK_BOX(vbox), lbl_title, FALSE, FALSE, 10);

    entry_name = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry_name), "닉네임");
    gtk_box_pack_start(GTK_BOX(vbox), entry_name, FALSE, FALSE, 0);

    entry_ip = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry_ip), "서버 IP (127.0.0.1)");
    gtk_entry_set_text(GTK_ENTRY(entry_ip), "127.0.0.1");
    gtk_box_pack_start(GTK_BOX(vbox), entry_ip, FALSE, FALSE, 0);

    entry_port = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry_port), "포트 (8080)");
    gtk_entry_set_text(GTK_ENTRY(entry_port), "8080");
    gtk_box_pack_start(GTK_BOX(vbox), entry_port, FALSE, FALSE, 0);

    GtkWidget *btn_connect = gtk_button_new_with_label("채팅방 입장");
    g_signal_connect(btn_connect, "clicked", G_CALLBACK(on_connect_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(vbox), btn_connect, FALSE, FALSE, 10);

    gtk_widget_show_all(login_window);
    gtk_main();
}
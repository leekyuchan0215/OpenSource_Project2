#ifndef UI_H
#define UI_H

#include "common.h"

// UI 초기화 및 실행
void activate_ui(int argc, char *argv[]);

// 외부에서 UI를 갱신하기 위한 함수들
void add_system_msg(const char *msg);
void add_chat_bubble(const char *name, const char *msg, int is_mine);
void add_file_download_btn(const char *filename);

#endif
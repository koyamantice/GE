#include "WindowAPI.h"
#include<Windows.h>


LRESULT  WindowAPI::WindowProc(HWND hwnd, UINT msg, WPARAM wparm, LPARAM lparam) {
	//メッセージで分岐
	switch (msg) {
	case WM_DESTROY://ウィンドウが破壊された
		PostQuitMessage(0);//OSに対して、アプリの終了を伝える
		return 0;
	}
	return DefWindowProc(hwnd, msg, wparm, lparam);//標準の処理を行う
}


void WindowAPI::Initialize() {

	w.cbSize = sizeof(WNDCLASSEX);
	w.lpfnWndProc = (WNDPROC)WindowProc;
	w.lpszClassName= L"DirectXGame";
	w.hInstance = GetModuleHandle(nullptr);
	w.hCursor = LoadCursor(NULL,IDC_ARROW);

	RegisterClassEx(&w);

	RECT wrc = {0,0,window_width,window_height};
	AdjustWindowRect(&wrc,WS_OVERLAPPEDWINDOW,false);

	//ウィンドウオブジェクトの生成
	hwnd = CreateWindow(w.lpszClassName,//クラス名
		L"DirectXGame",//タイトルバーの文字
		WS_OVERLAPPEDWINDOW,//標準的なウィンドウスタイル
		CW_USEDEFAULT,//表示X座標(OSに任せる)
		CW_USEDEFAULT,//表示Y座標(OSに任せる)
		wrc.right - wrc.left,//ウィンドウ横幅
		wrc.bottom - wrc.top,//ウィンドウ縦幅
		nullptr,//親ウィンドウハンドル
		nullptr,//メニューハンドル
		w.hInstance,//呼び出しアプリケーションハンドル
		nullptr);//オプション

	ShowWindow(hwnd, SW_SHOW);


}

void WindowAPI::Finalize() {
UnregisterClass(w.lpszClassName, w.hInstance);


}

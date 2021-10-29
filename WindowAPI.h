#pragma once
class WindowAPI {
public://’è”
	static const int window_width = 1280;
	static const int window_height = 720;

public:
	static LRESULT WindowProc(HWND hwnd, UINT msg, WPARAM wparm, LPARAM lparam);
private:
	WNDCLASSEX w{};
public://ƒƒ“ƒoŠÖ”
		//‰Šú‰»
		void Initialize();
		//XV
		//void Update();

		void Finalize();
		HINSTANCE GetHInstance() { return w.hInstance; }
		HWND GetHwnd() { return hwnd; }
private:
	HWND hwnd = nullptr;
};


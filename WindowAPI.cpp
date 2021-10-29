#include "WindowAPI.h"
#include<Windows.h>


LRESULT  WindowAPI::WindowProc(HWND hwnd, UINT msg, WPARAM wparm, LPARAM lparam) {
	//���b�Z�[�W�ŕ���
	switch (msg) {
	case WM_DESTROY://�E�B���h�E���j�󂳂ꂽ
		PostQuitMessage(0);//OS�ɑ΂��āA�A�v���̏I����`����
		return 0;
	}
	return DefWindowProc(hwnd, msg, wparm, lparam);//�W���̏������s��
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

	//�E�B���h�E�I�u�W�F�N�g�̐���
	hwnd = CreateWindow(w.lpszClassName,//�N���X��
		L"DirectXGame",//�^�C�g���o�[�̕���
		WS_OVERLAPPEDWINDOW,//�W���I�ȃE�B���h�E�X�^�C��
		CW_USEDEFAULT,//�\��X���W(OS�ɔC����)
		CW_USEDEFAULT,//�\��Y���W(OS�ɔC����)
		wrc.right - wrc.left,//�E�B���h�E����
		wrc.bottom - wrc.top,//�E�B���h�E�c��
		nullptr,//�e�E�B���h�E�n���h��
		nullptr,//���j���[�n���h��
		w.hInstance,//�Ăяo���A�v���P�[�V�����n���h��
		nullptr);//�I�v�V����

	ShowWindow(hwnd, SW_SHOW);


}

void WindowAPI::Finalize() {
UnregisterClass(w.lpszClassName, w.hInstance);


}

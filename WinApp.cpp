#include "WinApp.h"

LRESULT WinApp::WindowProc(HWND hwnd, UINT msg, WPARAM wparm, LPARAM lparam) {
	//���b�Z�[�W�ŕ���
	switch (msg) {
	case WM_DESTROY://�E�B���h�E���j�󂳂ꂽ
		PostQuitMessage(0);//OS�ɑ΂��āA�A�v���̏I����`����
		return 0;
	}
	return DefWindowProc(hwnd, msg, wparm, lparam);//�W���̏������s��
}


void WinApp::Initialize() {

	w.cbSize = sizeof(WNDCLASSEX);
	w.lpfnWndProc = (WNDPROC)WindowProc;//�E�B���h�E�v���V�[�W����ݒ�
	w.lpszClassName = L"DirectXGame";//�E�B���h�E�N���X��
	w.hInstance = GetModuleHandle(nullptr);//�E�B���h�E�n���h��
	w.hCursor = LoadCursor(NULL, IDC_ARROW);//�J�[�\���w��

	//�E�B���h�E�N���X��OS�ɓo�^
	RegisterClassEx(&w);
	//�E�B���h�E�T�C�Y{X���W Y���W�@�����@�c��}
	RECT wrc = { 0,0,window_width,window_height };
	AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);//�����ŃT�C�Y�␳

	//�E�B���h�E�I�u�W�F�N�g�̐���
	/*HWND*/ hwnd = CreateWindow(w.lpszClassName,//�N���X��
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

void WinApp::Update() {
}

bool WinApp::ProcessMessage() {

	MSG msg{};//���b�Z�[�W
			//���b�Z�[�W������?
	if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
		TranslateMessage(&msg);//�L�[���̓��b�Z�[�W�̏���
		DispatchMessage(&msg);//�v���V�[�W���Ƀ��b�Z�[�W�𑗂�
	}
	//x�{�^���ŏI�����b�Z�[�W��������Q�[�����[�v�𔲂���
	if (msg.message == WM_QUIT) {
		return true;
	}

	return false;
}

void WinApp::Finalize() {
	UnregisterClass(w.lpszClassName, w.hInstance);

}
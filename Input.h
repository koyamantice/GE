#pragma once

#include <Windows.h>
#include <wrl.h>
#include "WinApp.h"

#define DIRECTINPUT_VERSION     0x0800          // DirectInput�̃o�[�W�����w��
#include <dinput.h>

/// ����
class Input {
private: // �G�C���A�X
	// Microsoft::WRL::���ȗ�
	template <class T> using ComPtr = Microsoft::WRL::ComPtr<T>;
	WinApp* winApp = nullptr;
public: // �����o�֐�
	/// ������
	bool Initialize(WinApp* winApp);

	/// ���t���[������
	void Update();

	/// �L�[�̉������`�F�b�N
	bool PushKey(BYTE keyNumber);

	/// �L�[�̃g���K�[���`�F�b�N
	bool TriggerKey(BYTE keyNumber);

	/// �L�[�̃z�[���h���`�F�b�N
	bool HoldKey(BYTE keyNumber);

private: // �����o�ϐ�
	ComPtr<IDirectInput8> dinput;
	ComPtr<IDirectInputDevice8> devkeyboard;
	BYTE key[256] = {};
	BYTE keyPre[256] = {};
};


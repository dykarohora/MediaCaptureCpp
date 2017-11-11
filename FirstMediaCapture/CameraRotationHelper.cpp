#include "pch.h"
#include "CameraRotationHelper.h"

using namespace FirstMediaCapture;

CameraRotationHelper::CameraRotationHelper(EnclosureLocation^ cameraEnclosureLocation) {
	// �����f�B�X�v���C�̏����擾
	_displayInformation = DisplayInformation::GetForCurrentView();
	// �ȈՕ��ʃZ���T�[���擾
	_orientationSensor = SimpleOrientationSensor::GetDefault();
	// �J�����̓��ڈʒu���擾
	_cameraEnclosureLocation = cameraEnclosureLocation;

	// �J�������������ڋ@����������ʃZ���T�[�̌����ύX�C�x���g���T�u�X�N���C�u
	if (!IsEnclosureLocationExternal(_cameraEnclosureLocation) && _orientationSensor != nullptr) {
		_orientationSensor->OrientationChanged += 
			ref new TypedEventHandler<SimpleOrientationSensor^, SimpleOrientationSensorOrientationChangedEventArgs^>
			(this, &SimpleOrientationSensor_OrientationChanged);
	}
	_displayInformation->OrientationChanged +=
		ref new TypedEventHandler<DisplayInformation^, Object^>
		(this, &CameraRotationHelper::DisplayInformation_OrientationChanged);
}

// �J�����f�o�C�X���������ڋ@���A�O���ڑ��@���𔻒f����
// true:�O���ڑ� false:��������
bool CameraRotationHelper::IsEnclosureLocationExternal(EnclosureLocation^ enclosureLocation) {
	return (enclosureLocation == nullptr || enclosureLocation->Panel == Windows::Devices::Enumeration::Panel::Unknown);
}

SimpleOrientation CameraRotationHelper::GetUIOrientation() {
	if (IsEnclosureLocationExternal(_cameraEnclosureLocation)) {
		// �O���ڑ��@
		return SimpleOrientation::NotRotated;
	}
	// ��������
	// ���ʃZ���T�[����f�o�C�X�̌������擾����
	SimpleOrientation deviceOrientation = _orientationSensor != nullptr ? _orientationSensor->GetCurrentOrientation() : SimpleOrientation::NotRotated;
	//
	SimpleOrientation displayOrientation = ConvertDisplayOrientationToSimpleOrientation(_displayInformation->CurrentOrientation);
	// 
	return SubtractOrientation(displayOrientation, deviceOrientation);
}
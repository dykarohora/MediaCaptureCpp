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

// UI�̌�����SimpleOrientation�Ŏ擾����
SimpleOrientation CameraRotationHelper::GetUIOrientation() {
	if (IsEnclosureLocationExternal(_cameraEnclosureLocation)) {
		// �O���ڑ��@
		return SimpleOrientation::NotRotated;
	}
	// ��������
	// ���ʃZ���T�[����f�o�C�X�̌������擾���� - 1
	SimpleOrientation deviceOrientation = _orientationSensor != nullptr ? _orientationSensor->GetCurrentOrientation() : SimpleOrientation::NotRotated;
	// DisplayInformation����SimpleOrientation���擾���� - 2
	SimpleOrientation displayOrientation = ConvertDisplayOrientationToSimpleOrientation(_displayInformation->CurrentOrientation);
	// �f�B�X�v���C�̌�����LandScape�Ƀ��b�N����Ă���
	// �Ȃ̂�UI�̓f�o�C�X�����Ɉˑ��H�H�H
	return SubtractOrientations(displayOrientation, deviceOrientation);
}

// �ʐ^��r�f�I���t�@�C���ɕۑ�����Ƃ��Ƀ��^�f�[�^�ɖ��ߍ��ނ��߂Ɍ������擾����
SimpleOrientation CameraRotationHelper::GetCameraCaptureOrientation() {
	// �O���ڑ��@
	if (IsEnclosureLocationExternal(_cameraEnclosureLocation)) {
		return SimpleOrientation::NotRotated;
	}

	// ���ʃZ���T�[����f�o�C�X�̌������擾
	SimpleOrientation deviceOrientation = _orientationSensor != nullptr ? _orientationSensor->GetCurrentOrientation() : SimpleOrientation::NotRotated;
	// ���ʃZ���T�[�ƃJ�����̌����̍������擾
	SimpleOrientation result = SubtractOrientations(deviceOrientation, GetCameraOrientationRelativeToNariveOrientation());
	// �t�����g�J�����ł������ꍇ�͍��E���]����
	if (ShouldMirrorPreview()) {
		result = MirrorOrientation(result);
	}
	return result;
}

// �J�����v���r���[���̌����H�H
SimpleOrientation CameraRotationHelper::GetCameraPreviewOrientation() {
	if (IsEnclosureLocationExternal(_cameraEnclosureLocation)) {
		return SimpleOrientation::NotRotated;
	}

	// DisplayInformation����SimpleOrientation���擾(Landscape�ɌŒ肵�Ă邩��NotRotated??)
	SimpleOrientation result = ConvertDisplayOrientationToSimpleOrientation(_displayInformation->CurrentOrientation);
	// ??
	result = SubtractOrientations(result, GetCameraOrientationRelativeToNariveOrientation());
	// �t�����g�J�����ł������ꍇ�͍��E���]����
	if (ShouldMirrorPreview()) {
		result = MirrorOrientation(result);
	}
	return result;
}

// Exif�t���O�Ɏg����悤SimpleOrientation��ϊ�����
PhotoOrientation CameraRotationHelper::ConvertSimpleOrientationToPhotoOrientation(SimpleOrientation orientation) {
	switch (orientation)
	{
	case SimpleOrientation::Rotated90DegreesCounterclockwise:
		return PhotoOrientation::Rotate90;
	case SimpleOrientation::Rotated180DegreesCounterclockwise:
		return PhotoOrientation::Rotate180;
	case SimpleOrientation::Rotated270DegreesCounterclockwise:
		return PhotoOrientation::Rotate270;
	case SimpleOrientation::NotRotated:
	default:
		return PhotoOrientation::Normal;
	}
}

// SimpleOrientation����]�p�x���ɕϊ�����(���v����+�̉�]�Ƃ���)
int CameraRotationHelper::ConvertSimpleOrientationToClockwiseDegrees(SimpleOrientation orientation) {
	switch (orientation)
	{
	case SimpleOrientation::Rotated90DegreesCounterclockwise:
		return 270;
	case SimpleOrientation::Rotated180DegreesCounterclockwise:
		return 180;
	case SimpleOrientation::Rotated270DegreesCounterclockwise:
		return 90;
	case SimpleOrientation::NotRotated:
		return 0;
	default:
		break;
	}
}

// DisplayOrientation�̒l�����Ƃ�SimpleOrientation���擾����
SimpleOrientation CameraRotationHelper::ConvertDisplayOrientationToSimpleOrientation(DisplayOrientations orientation) {
	SimpleOrientation result;
	switch (orientation)
	{
	case DisplayOrientations::Landscape:
		result = SimpleOrientation::NotRotated;
		break;
	case DisplayOrientations::PortraitFlipped:
		result = SimpleOrientation::Rotated90DegreesCounterclockwise;
		break;
	case DisplayOrientations::LandscapeFlipped:
		result = SimpleOrientation::Rotated180DegreesCounterclockwise;
		break;
	case DisplayOrientations::Portrait:
	default:
		result = SimpleOrientation::Rotated270DegreesCounterclockwise;
		break;
	}
	// �f�t�H���g�̌�����Portrait�ł������ꍇ�́A�����v90�x��]��������
	if (_displayInformation->NativeOrientation == DisplayOrientations::Portrait) {
		result = AddOrientations(result, SimpleOrientation::Rotated90DegreesCounterclockwise);
	}

	return result;
}

// ���E���]����SimpleOrientation���擾����
SimpleOrientation CameraRotationHelper::MirrorOrientation(SimpleOrientation orientation) {
	switch (orientation)
	{
	case SimpleOrientation::Rotated90DegreesCounterclockwise:
		return SimpleOrientation::Rotated270DegreesCounterclockwise;
	case SimpleOrientation::Rotated270DegreesCounterclockwise:
		return SimpleOrientation::Rotated90DegreesCounterclockwise;
	}
	return orientation;
}

// 2��SimpleOrientation�����Z����(ex.270+180=90 (450-360) )
SimpleOrientation CameraRotationHelper::AddOrientations(SimpleOrientation a, SimpleOrientation b) {
	int aRot = ConvertSimpleOrientationToClockwiseDegrees(a);
	int bRot = ConvertSimpleOrientationToClockwiseDegrees(b);
	int result = (aRot + bRot) % 360;

	return ConvertClockwiseDegreesToSimpleOrientation(result);
}

// 2��SimpleOrientation�����Z����
SimpleOrientation CameraRotationHelper::SubtractOrientations(SimpleOrientation a, SimpleOrientation b) {
	int aRot = ConvertSimpleOrientationToClockwiseDegrees(a);
	int bRot = ConvertSimpleOrientationToClockwiseDegrees(b);
	// �����Ƃ��Ď擾
	int result = (360 + (aRot - bRot)) % 360;
	return ConvertClockwiseDegreesToSimpleOrientation(result);
}

VideoRotation CameraRotationHelper::ConvertSimpleOrientationToVideoRotation(SimpleOrientation orientation) {
	switch (orientation)
	{
	case SimpleOrientation::Rotated90DegreesCounterclockwise:
		return VideoRotation::Clockwise270Degrees;
	case SimpleOrientation::Rotated180DegreesCounterclockwise:
		return VideoRotation::Clockwise180Degrees;
	case SimpleOrientation::Rotated270DegreesCounterclockwise:
		return VideoRotation::Clockwise90Degrees;
	case SimpleOrientation::NotRotated:
	default:
		return VideoRotation::None;
	}
}

// ��]�p�x����SimpleOrientation�ɕϊ�����(���v��������+�̉�]�Ƃ���)
SimpleOrientation CameraRotationHelper::ConvertClockwiseDegreesToSimpleOrientation(int orientation) {
	switch (orientation)
	{
	case 270:
		return SimpleOrientation::Rotated90DegreesCounterclockwise;
	case 180:
		return SimpleOrientation::Rotated180DegreesCounterclockwise;
	case 90:
		return SimpleOrientation::Rotated270DegreesCounterclockwise;
	case 0:
	default:
		return SimpleOrientation::NotRotated;
	}
}

//
void CameraRotationHelper::SimpleOrientationSensor_OrientationChanged(SimpleOrientationSensor^ sender, SimpleOrientationSensorOrientationChangedEventArgs^ args) {
	if (args->Orientation != SimpleOrientation::Faceup && args->Orientation != SimpleOrientation::Facedown) {
		OrientationChanged(this, false);
	}
}

// 
void CameraRotationHelper::DisplayInformation_OrientationChanged(DisplayInformation^ sender, Object^ ref) {
	OrientationChanged(this, true);
}

// �t�����g�J�����ł������ꍇ�͍��E���]����
bool CameraRotationHelper::ShouldMirrorPreview() {
	return (_cameraEnclosureLocation->Panel == Windows::Devices::Enumeration::Panel::Front);
}

// �J�����f�o�C�X�̌������擾�H�H
SimpleOrientation CameraRotationHelper::GetCameraOrientationRelativeToNariveOrientation() {
	// �J�����f�o�C�X�̌�����SimpleOrientation�ɕϊ��H�H
	SimpleOrientation enclosureAngle = ConvertClockwiseDegreesToSimpleOrientation((int)_cameraEnclosureLocation->RotationAngleInDegreesClockwise);

	if (_displayInformation->NativeOrientation == DisplayOrientations::Portrait && !IsEnclosureLocationExternal(_cameraEnclosureLocation)) {
		enclosureAngle = AddOrientations(SimpleOrientation::Rotated90DegreesCounterclockwise, enclosureAngle);
	}

	return enclosureAngle;

}
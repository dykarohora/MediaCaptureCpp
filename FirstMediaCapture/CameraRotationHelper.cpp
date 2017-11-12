#include "pch.h"
#include "CameraRotationHelper.h"

using namespace FirstMediaCapture;

CameraRotationHelper::CameraRotationHelper(EnclosureLocation^ cameraEnclosureLocation) {
	// 物理ディスプレイの情報を取得
	_displayInformation = DisplayInformation::GetForCurrentView();
	// 簡易方位センサーを取得
	_orientationSensor = SimpleOrientationSensor::GetDefault();
	// カメラの搭載位置を取得
	_cameraEnclosureLocation = cameraEnclosureLocation;

	// カメラが内部搭載機だったら方位センサーの向き変更イベントをサブスクライブ
	if (!IsEnclosureLocationExternal(_cameraEnclosureLocation) && _orientationSensor != nullptr) {
		_orientationSensor->OrientationChanged += 
			ref new TypedEventHandler<SimpleOrientationSensor^, SimpleOrientationSensorOrientationChangedEventArgs^>
			(this, &SimpleOrientationSensor_OrientationChanged);
	}
	_displayInformation->OrientationChanged +=
		ref new TypedEventHandler<DisplayInformation^, Object^>
		(this, &CameraRotationHelper::DisplayInformation_OrientationChanged);
}

// カメラデバイスが内部搭載機か、外部接続機かを判断する
// true:外部接続 false:内部搭載
bool CameraRotationHelper::IsEnclosureLocationExternal(EnclosureLocation^ enclosureLocation) {
	return (enclosureLocation == nullptr || enclosureLocation->Panel == Windows::Devices::Enumeration::Panel::Unknown);
}

// UIの向きをSimpleOrientationで取得する
SimpleOrientation CameraRotationHelper::GetUIOrientation() {
	if (IsEnclosureLocationExternal(_cameraEnclosureLocation)) {
		// 外部接続機
		return SimpleOrientation::NotRotated;
	}
	// 内部搭載
	// 方位センサーからデバイスの向きを取得する - 1
	SimpleOrientation deviceOrientation = _orientationSensor != nullptr ? _orientationSensor->GetCurrentOrientation() : SimpleOrientation::NotRotated;
	// DisplayInformationからSimpleOrientationを取得する - 2
	SimpleOrientation displayOrientation = ConvertDisplayOrientationToSimpleOrientation(_displayInformation->CurrentOrientation);
	// ディスプレイの向きはLandScapeにロックされている
	// なのでUIはデバイス向きに依存？？？
	return SubtractOrientations(displayOrientation, deviceOrientation);
}

// 写真やビデオをファイルに保存するときにメタデータに埋め込むために向きを取得する
SimpleOrientation CameraRotationHelper::GetCameraCaptureOrientation() {
	// 外部接続機
	if (IsEnclosureLocationExternal(_cameraEnclosureLocation)) {
		return SimpleOrientation::NotRotated;
	}

	// 方位センサーからデバイスの向きを取得
	SimpleOrientation deviceOrientation = _orientationSensor != nullptr ? _orientationSensor->GetCurrentOrientation() : SimpleOrientation::NotRotated;
	// 方位センサーとカメラの向きの差分を取得
	SimpleOrientation result = SubtractOrientations(deviceOrientation, GetCameraOrientationRelativeToNariveOrientation());
	// フロントカメラであった場合は左右反転する
	if (ShouldMirrorPreview()) {
		result = MirrorOrientation(result);
	}
	return result;
}

// カメラプレビュー時の向き？？
SimpleOrientation CameraRotationHelper::GetCameraPreviewOrientation() {
	if (IsEnclosureLocationExternal(_cameraEnclosureLocation)) {
		return SimpleOrientation::NotRotated;
	}

	// DisplayInformationからSimpleOrientationを取得(Landscapeに固定してるからNotRotated??)
	SimpleOrientation result = ConvertDisplayOrientationToSimpleOrientation(_displayInformation->CurrentOrientation);
	// ??
	result = SubtractOrientations(result, GetCameraOrientationRelativeToNariveOrientation());
	// フロントカメラであった場合は左右反転する
	if (ShouldMirrorPreview()) {
		result = MirrorOrientation(result);
	}
	return result;
}

// Exifフラグに使えるようSimpleOrientationを変換する
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

// SimpleOrientationを回転角度数に変換する(時計回りを+の回転とする)
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

// DisplayOrientationの値をもとにSimpleOrientationを取得する
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
	// デフォルトの向きがPortraitであった場合は、反時計90度回転を加える
	if (_displayInformation->NativeOrientation == DisplayOrientations::Portrait) {
		result = AddOrientations(result, SimpleOrientation::Rotated90DegreesCounterclockwise);
	}

	return result;
}

// 左右反転したSimpleOrientationを取得する
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

// 2つのSimpleOrientationを加算する(ex.270+180=90 (450-360) )
SimpleOrientation CameraRotationHelper::AddOrientations(SimpleOrientation a, SimpleOrientation b) {
	int aRot = ConvertSimpleOrientationToClockwiseDegrees(a);
	int bRot = ConvertSimpleOrientationToClockwiseDegrees(b);
	int result = (aRot + bRot) % 360;

	return ConvertClockwiseDegreesToSimpleOrientation(result);
}

// 2つのSimpleOrientationを減算する
SimpleOrientation CameraRotationHelper::SubtractOrientations(SimpleOrientation a, SimpleOrientation b) {
	int aRot = ConvertSimpleOrientationToClockwiseDegrees(a);
	int bRot = ConvertSimpleOrientationToClockwiseDegrees(b);
	// 正数として取得
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

// 回転角度数をSimpleOrientationに変換する(時計回り方向を+の回転とする)
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

// フロントカメラであった場合は左右反転する
bool CameraRotationHelper::ShouldMirrorPreview() {
	return (_cameraEnclosureLocation->Panel == Windows::Devices::Enumeration::Panel::Front);
}

// カメラデバイスの向きを取得？？
SimpleOrientation CameraRotationHelper::GetCameraOrientationRelativeToNariveOrientation() {
	// カメラデバイスの向きをSimpleOrientationに変換？？
	SimpleOrientation enclosureAngle = ConvertClockwiseDegreesToSimpleOrientation((int)_cameraEnclosureLocation->RotationAngleInDegreesClockwise);

	if (_displayInformation->NativeOrientation == DisplayOrientations::Portrait && !IsEnclosureLocationExternal(_cameraEnclosureLocation)) {
		enclosureAngle = AddOrientations(SimpleOrientation::Rotated90DegreesCounterclockwise, enclosureAngle);
	}

	return enclosureAngle;

}
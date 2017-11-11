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

SimpleOrientation CameraRotationHelper::GetUIOrientation() {
	if (IsEnclosureLocationExternal(_cameraEnclosureLocation)) {
		// 外部接続機
		return SimpleOrientation::NotRotated;
	}
	// 内部搭載
	// 方位センサーからデバイスの向きを取得する
	SimpleOrientation deviceOrientation = _orientationSensor != nullptr ? _orientationSensor->GetCurrentOrientation() : SimpleOrientation::NotRotated;
	//
	SimpleOrientation displayOrientation = ConvertDisplayOrientationToSimpleOrientation(_displayInformation->CurrentOrientation);
	// 
	return SubtractOrientation(displayOrientation, deviceOrientation);
}
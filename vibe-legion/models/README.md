Сюда кладутся ONNX-модели (не хранятся в git). Скачать и проверить SHA-256:

    powershell -ExecutionPolicy Bypass -File scripts\fetch-deps.ps1

Нужны: palm_detection_mediapipe_2023feb.onnx, handpose_estimation_mediapipe_2023feb.onnx,
face_detection_yunet_2023mar.onnx, face_recognition_sface_2021dec_int8.onnx (OpenCV Zoo).

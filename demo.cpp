
#include <windows.h>

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    MessageBoxW(
        nullptr,
        L"RogueByte Demo\nExtracted successfully from the image!",
        L"Steganography Lab",
        MB_OK | MB_ICONINFORMATION
    );

    return 0;
}

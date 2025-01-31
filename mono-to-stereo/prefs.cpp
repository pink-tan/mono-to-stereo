// prefs.cpp

#include "common.h"

#define DEFAULT_BUFFER_MS 64
// #define DEFAULT_INPUT_DIVICE_NAME L"Digital Audio Interface (USB Digital Audio)"
// for Windows Japanese Language Shift-JIS
#define DEFAULT_INPUT_DIVICE_NAME_STARTS_WITH L"デジタル オーディオ インターフェイス"
#define DEFAULT_OUTPUT_DIVICE_NAME_STARTS_WITH L"CABLE Input (VB-Audio Virtual Cable)"

void usage(LPCWSTR exe);
HRESULT get_default_device(IMMDevice **ppMMDevice);
HRESULT list_devices();
HRESULT list_devices_with_direction(EDataFlow direction, const wchar_t *direction_label);
HRESULT get_specific_device(LPCWSTR szLongName, EDataFlow direction, IMMDevice **ppMMDevice, bool bStrictCompare = true);

void usage(LPCWSTR exe) {
    LOG(
        L"mono-to-stereo v%s\n"
        L"\n"
        L"%ls -?\n"
		L"%ls --list-devices\n"
		L"%ls [--in-device \"Device long name\"] [--out-device \"Device long name\"] [--buffer-size 128] [--no-skip-first-sample]\n"
        L"\n"
        L"    -? prints this message.\n"
		L"    -l same as --list-devices.\n"
		L"    -i same as --in-device.\n"
		L"    -o same as --out-device.\n"
		L"    -b same as --buffer-size.\n"
		L"    --list-devices displays the long names of all active capture and render devices.\n"
		L"    --in-device captures from the specified device to capture\n"
		L"      (\"%s\" if omitted)\n"
        L"    --out-device device to stream stereo audio to\n"
		L"      (\"%s\" or default if omitted)\n"
        L"    --buffer-size set the size of the audio buffer, in milliseconds\n"
		L"      (default to %dms)\n"
        L"    --no-skip-first-sample do not skip the first channel sample",
        VERSION, exe, exe, exe,
		DEFAULT_INPUT_DIVICE_NAME_STARTS_WITH,
		DEFAULT_OUTPUT_DIVICE_NAME_STARTS_WITH,
		DEFAULT_BUFFER_MS
    );
}

CPrefs::CPrefs(int argc, LPCWSTR argv[], HRESULT &hr)
    : m_pMMInDevice(NULL)
    , m_pMMOutDevice(NULL)
    , m_iBufferMs(DEFAULT_BUFFER_MS)
    , m_bSkipFirstSample(true)
{
    switch (argc) {
    case 2:
        if (0 == _wcsicmp(argv[1], L"-?") || 0 == _wcsicmp(argv[1], L"/?")) {
            // print usage but don't actually capture
            hr = S_FALSE;
            usage(argv[0]);
            return;
        }
		// --list-devices, -l
		else if ((0 == _wcsicmp(argv[1], L"--list-devices")) || (0 == _wcsicmp(argv[1], L"-l"))) {
            // list the devices but don't actually capture
            hr = list_devices();

            // don't actually play
            if (S_OK == hr) {
                hr = S_FALSE;
                return;
            }
        }
        // intentional fallthrough

    default:
        // loop through arguments and parse them
        for (int i = 1; i < argc; i++) {

            // --in-device, -i
            if ((0 == _wcsicmp(argv[i], L"--in-device")) || (0 == _wcsicmp(argv[i], L"-i"))) {
                if (NULL != m_pMMInDevice) {
                    ERR(L"%s", L"Only one --device switch is allowed");
                    hr = E_INVALIDARG;
                    return;
                }

                if (i++ == argc) {
                    ERR(L"%s", L"--device switch requires an argument");
                    hr = E_INVALIDARG;
                    return;
                }

                hr = get_specific_device(argv[i], eCapture, &m_pMMInDevice);
                if (FAILED(hr)) {
                    return;
                }

                continue;
            }

            // --out-device, -o
            if ((0 == _wcsicmp(argv[i], L"--out-device")) || (0 == _wcsicmp(argv[i], L"-o")) ){
                if (NULL != m_pMMOutDevice) {
                    ERR(L"%s", L"Only one --device switch is allowed");
                    hr = E_INVALIDARG;
                    return;
                }

                if (i++ == argc) {
                    ERR(L"%s", L"--device switch requires an argument");
                    hr = E_INVALIDARG;
                    return;
                }

                hr = get_specific_device(argv[i], eRender, &m_pMMOutDevice);
                if (FAILED(hr)) {
                    return;
                }

                continue;
            }

            // --buffer-size, -b
            if ((0 == _wcsicmp(argv[i], L"--buffer-size")) || (0 == _wcsicmp(argv[i], L"-b"))) {
                if (i++ == argc) {
                    ERR(L"%s", L"--buffer-size switch requires an argument");
                    hr = E_INVALIDARG;
                    return;
                }

                m_iBufferMs = _wtoi(argv[i]);
                if (m_iBufferMs <= 0) {
                    ERR(L"%s", L"invalid buffer size given");
                    hr = E_INVALIDARG;
                    return;
                }

                continue;
            }

            // --no-skip-first-sample
            if (0 == _wcsicmp(argv[i], L"--no-skip-first-sample")) {
                m_bSkipFirstSample = false;
                continue;
            }

            ERR(L"Invalid argument %ls", argv[i]);
            hr = E_INVALIDARG;
            return;
        }

        // open default device if not specified
        if (NULL == m_pMMInDevice) {
            hr = get_specific_device(DEFAULT_INPUT_DIVICE_NAME_STARTS_WITH, eCapture, &m_pMMInDevice, false);
            if (FAILED(hr)) {
                return;
            }
        }

        // open default device if not specified
        if (NULL == m_pMMOutDevice) {
			hr = get_specific_device(DEFAULT_OUTPUT_DIVICE_NAME_STARTS_WITH, eRender, &m_pMMOutDevice, false);
			if (FAILED(hr)) {
				hr = get_default_device(&m_pMMOutDevice);
				if (FAILED(hr)) {
					return;
				}
			}
        }
    }
}

CPrefs::~CPrefs() {
    if (NULL != m_pMMInDevice) {
        m_pMMInDevice->Release();
    }

    if (NULL != m_pMMOutDevice) {
        m_pMMOutDevice->Release();
    }
}

HRESULT get_default_device(IMMDevice **ppMMDevice) {
    HRESULT hr = S_OK;
    IMMDeviceEnumerator *pMMDeviceEnumerator;

    // activate a device enumerator
    hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator),
        (void**)&pMMDeviceEnumerator
    );
    if (FAILED(hr)) {
        ERR(L"CoCreateInstance(IMMDeviceEnumerator) failed: hr = 0x%08x", hr);
        return hr;
    }
    ReleaseOnExit releaseMMDeviceEnumerator(pMMDeviceEnumerator);

    // get the default render endpoint
    hr = pMMDeviceEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, ppMMDevice);
    if (FAILED(hr)) {
        ERR(L"IMMDeviceEnumerator::GetDefaultAudioEndpoint failed: hr = 0x%08x", hr);
        return hr;
    }

    return S_OK;
}

HRESULT list_devices() {
    HRESULT hr;

    hr = list_devices_with_direction(eRender, L"render");
    if (FAILED(hr)) {
        return hr;
    }

    LOG(L"");

    hr = list_devices_with_direction(eCapture, L"capture");
    if (FAILED(hr)) {
        return hr;
    }

    return hr;
}

HRESULT list_devices_with_direction(EDataFlow direction, const wchar_t *direction_label) {
    HRESULT hr = S_OK;

    // get an enumerator
    IMMDeviceEnumerator *pMMDeviceEnumerator;

    hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator),
        (void**)&pMMDeviceEnumerator
    );
    if (FAILED(hr)) {
        ERR(L"CoCreateInstance(IMMDeviceEnumerator) failed: hr = 0x%08x", hr);
        return hr;
    }
    ReleaseOnExit releaseMMDeviceEnumerator(pMMDeviceEnumerator);

    IMMDeviceCollection *pMMDeviceCollection;

    // get all the active render endpoints
    hr = pMMDeviceEnumerator->EnumAudioEndpoints(
        direction, DEVICE_STATE_ACTIVE, &pMMDeviceCollection
    );
    if (FAILED(hr)) {
        ERR(L"IMMDeviceEnumerator::EnumAudioEndpoints failed: hr = 0x%08x", hr);
        return hr;
    }
    ReleaseOnExit releaseMMDeviceCollection(pMMDeviceCollection);

    UINT count;
    hr = pMMDeviceCollection->GetCount(&count);
    if (FAILED(hr)) {
        ERR(L"IMMDeviceCollection::GetCount failed: hr = 0x%08x", hr);
        return hr;
    }
    LOG(L"Active %s endpoints found: %u", direction_label, count);

    for (UINT i = 0; i < count; i++) {
        IMMDevice *pMMDevice;

        // get the "n"th device
        hr = pMMDeviceCollection->Item(i, &pMMDevice);
        if (FAILED(hr)) {
            ERR(L"IMMDeviceCollection::Item failed: hr = 0x%08x", hr);
            return hr;
        }
        ReleaseOnExit releaseMMDevice(pMMDevice);

        // open the property store on that device
        IPropertyStore *pPropertyStore;
        hr = pMMDevice->OpenPropertyStore(STGM_READ, &pPropertyStore);
        if (FAILED(hr)) {
            ERR(L"IMMDevice::OpenPropertyStore failed: hr = 0x%08x", hr);
            return hr;
        }
        ReleaseOnExit releasePropertyStore(pPropertyStore);

        // get the long name property
        PROPVARIANT pv; PropVariantInit(&pv);
        hr = pPropertyStore->GetValue(PKEY_Device_FriendlyName, &pv);
        if (FAILED(hr)) {
            ERR(L"IPropertyStore::GetValue failed: hr = 0x%08x", hr);
            return hr;
        }
        PropVariantClearOnExit clearPv(&pv);

        if (VT_LPWSTR != pv.vt) {
            ERR(L"PKEY_Device_FriendlyName variant type is %u - expected VT_LPWSTR", pv.vt);
            return E_UNEXPECTED;
        }

        LOG(L"    %ls", pv.pwszVal);
    }

    return S_OK;
}

HRESULT get_specific_device(LPCWSTR szLongName, EDataFlow direction, IMMDevice **ppMMDevice, bool bStrictCompare) {
    HRESULT hr = S_OK;

    *ppMMDevice = NULL;

    // get an enumerator
    IMMDeviceEnumerator *pMMDeviceEnumerator;

    hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator),
        (void**)&pMMDeviceEnumerator
    );
    if (FAILED(hr)) {
        ERR(L"CoCreateInstance(IMMDeviceEnumerator) failed: hr = 0x%08x", hr);
        return hr;
    }
    ReleaseOnExit releaseMMDeviceEnumerator(pMMDeviceEnumerator);

    IMMDeviceCollection *pMMDeviceCollection;

    // get all the active render endpoints
    hr = pMMDeviceEnumerator->EnumAudioEndpoints(
        direction, DEVICE_STATE_ACTIVE, &pMMDeviceCollection
    );
    if (FAILED(hr)) {
        ERR(L"IMMDeviceEnumerator::EnumAudioEndpoints failed: hr = 0x%08x", hr);
        return hr;
    }
    ReleaseOnExit releaseMMDeviceCollection(pMMDeviceCollection);

    UINT count;
    hr = pMMDeviceCollection->GetCount(&count);
    if (FAILED(hr)) {
        ERR(L"IMMDeviceCollection::GetCount failed: hr = 0x%08x", hr);
        return hr;
    }

    for (UINT i = 0; i < count; i++) {
        IMMDevice *pMMDevice;

        // get the "n"th device
        hr = pMMDeviceCollection->Item(i, &pMMDevice);
        if (FAILED(hr)) {
            ERR(L"IMMDeviceCollection::Item failed: hr = 0x%08x", hr);
            return hr;
        }
        ReleaseOnExit releaseMMDevice(pMMDevice);

        // open the property store on that device
        IPropertyStore *pPropertyStore;
        hr = pMMDevice->OpenPropertyStore(STGM_READ, &pPropertyStore);
        if (FAILED(hr)) {
            ERR(L"IMMDevice::OpenPropertyStore failed: hr = 0x%08x", hr);
            return hr;
        }
        ReleaseOnExit releasePropertyStore(pPropertyStore);

        // get the long name property
        PROPVARIANT pv; PropVariantInit(&pv);
        hr = pPropertyStore->GetValue(PKEY_Device_FriendlyName, &pv);
        if (FAILED(hr)) {
            ERR(L"IPropertyStore::GetValue failed: hr = 0x%08x", hr);
            return hr;
        }
        PropVariantClearOnExit clearPv(&pv);

        if (VT_LPWSTR != pv.vt) {
            ERR(L"PKEY_Device_FriendlyName variant type is %u - expected VT_LPWSTR", pv.vt);
            return E_UNEXPECTED;
        }

        // is it a match?
        bool bFind = false;
        if (bStrictCompare && 0 == _wcsicmp(pv.pwszVal, szLongName)) {
            bFind = true;
        } else
        if (!bStrictCompare && NULL != wcsstr(pv.pwszVal, szLongName)) {
            bFind = true;
        }

		if (bFind) {
			// did we already find it?
			if (NULL == *ppMMDevice) {
				*ppMMDevice = pMMDevice;
				pMMDevice->AddRef();
			}
			else {
				ERR(L"Found (at least) two devices named %ls", szLongName);
				return E_UNEXPECTED;
			}
		}
    }

    if (NULL == *ppMMDevice) {
        ERR(L"Could not find a device named %ls", szLongName);
        return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
    }

    return S_OK;
}

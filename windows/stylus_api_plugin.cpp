#include "include/stylus_api/stylus_api_plugin.h"

// This must be included before many other Windows headers.
#include <windows.h>

// For getPlatformVersion; remove unless needed for your plugin implementation.
#include <VersionHelpers.h>

#include <flutter/method_channel.h>
#include <flutter/plugin_registrar_windows.h>
#include <flutter/standard_method_codec.h>

#include <map>
#include <memory>
#include <sstream>

#include <ole2.h>
#include <rtscom.h>
#include <rtscom_i.c>

#pragma warning( disable : 6001 )
#pragma warning( disable : 6011 )
#pragma warning( disable : 26451 )
#pragma warning( disable : 4189 )

typedef unsigned long	uint32;

// Some constants
const uint32 c_WinWidth = 1024;
const uint32 c_WinHeight = 768;
const char* c_WinCaption = "Pressure test SDL window";


// Current settings
bool g_bInverted = false;
float g_Pressure = 0.0f;
int g_MouseX;
int g_MouseY;
int g_Trace;
float ScaleX, ScaleY;

// channels
HWND g_NativeWindows;

std::shared_ptr<flutter::MethodChannel<flutter::EncodableValue>> g_Channel;

// Stylus handler
bool g_bTriedToCreateRTSHandler = false;
class CRTSEventHandler* g_pRTSHandler = NULL;
IRealTimeStylus* g_pRTSStylus = NULL;

// Tablet context to pressure sensitivity
const uint32 c_nMaxContexts = 4;
uint32	g_nContexts = 0;

struct
{
	TABLET_CONTEXT_ID	iTabletContext;
	uint32				iPressure;
	float				PressureRcp;
} g_lContexts[c_nMaxContexts];

struct
{
	TABLET_CONTEXT_ID	iTabletContext;
	uint32				iX;
	uint32				iY;
	uint32				iWidth;
	uint32				iHeight;
} g_lXContexts[c_nMaxContexts];

//-------------------------------------------------------------------------------------------------
// RealTimeStylus syncevent plugin
class CRTSEventHandler : public IStylusSyncPlugin
{
public:

	CRTSEventHandler() : m_nRef(1), m_pPunkFTMarshaller(NULL) { }
	virtual ~CRTSEventHandler()
	{
		if (m_pPunkFTMarshaller != NULL)
			m_pPunkFTMarshaller->Release();
	}

	// IStylusSyncPlugin inherited methods

	// Methods whose data we use
	STDMETHOD(Packets)(IRealTimeStylus* pStylus, const StylusInfo* pStylusInfo, ULONG nPackets, ULONG nPacketBuf, LONG* pPackets, ULONG* nOutPackets, LONG** ppOutPackets)
	{
		g_Trace += 1;
		uint32 iCtx = c_nMaxContexts;
		for (uint32 i = 0; i < g_nContexts; i++)
		{
			if (g_lContexts[i].iTabletContext == pStylusInfo->tcid)
			{
				iCtx = i;
				break;
			}
		}

		// If we are not getting pressure values, just ignore it
		// if (iCtx >= c_nMaxContexts)
		//	return S_OK;

		// Get properties
		ULONG nProps = nPacketBuf / nPackets;

		// Pointer to the last packet in the batch - we don't really care about intermediates for this example
		LONG* pLastPacket = pPackets + (nPacketBuf - nProps);

		// g_Pressure = pLastPacket[g_lContexts[iCtx].iPressure] * g_lContexts[iCtx].PressureRcp;
		// g_bInverted = (pStylusInfo->bIsInvertedCursor != 0);

		ULONG x = pLastPacket[g_lXContexts[0].iX];
		ULONG y = pLastPacket[g_lXContexts[0].iY];
		ULONG width = pLastPacket[g_lXContexts[0].iWidth];
		ULONG height = pLastPacket[g_lXContexts[0].iHeight];

		std::ostringstream args_stream;
		args_stream << x * ScaleX << "," << y * ScaleY;
		// args_stream << x * 0.0755905509 << "," << y * 0.0755905509; // << "," << ScaleX << "," << ScaleY << "," << width << "," << height;
		// args_stream << "iX = " << g_lXContexts[0].iX << ", " << "iY = " << g_lXContexts[0].iY << ",";
		// args_stream << "iWidth = " << g_lXContexts[0].iWidth << ", " << "iHeight = " << g_lXContexts[0].iHeight << ",";
		g_Channel->InvokeMethod("Packets", std::make_unique<flutter::EncodableValue>(flutter::EncodableValue(args_stream.str())));

		return S_OK;
	}
	STDMETHOD(DataInterest)(RealTimeStylusDataInterest* pEventInterest)
	{
		g_Channel->InvokeMethod("DataInterest", nullptr);
		*pEventInterest = (RealTimeStylusDataInterest)(RTSDI_AllData);
		return S_OK;
	}

	// Methods you can add if you need the alerts - don't forget to change DataInterest!
	STDMETHOD(StylusDown)(IRealTimeStylus*, const StylusInfo* pStylusInfo, ULONG nPackets, LONG* _pPackets, LONG**) {
		uint32 iCtx = c_nMaxContexts;
		for (uint32 i = 0; i < g_nContexts; i++)
		{
			if (g_lContexts[i].iTabletContext == pStylusInfo->tcid)
			{
				iCtx = i;
				break;
			}
		}

		// If we are not getting pressure values, just ignore it
		// if (iCtx >= c_nMaxContexts)
		//	return S_OK;

		// g_Pressure = pLastPacket[g_lContexts[iCtx].iPressure] * g_lContexts[iCtx].PressureRcp;
		// g_bInverted = (pStylusInfo->bIsInvertedCursor != 0);

		ULONG x = _pPackets[g_lXContexts[0].iX];
		ULONG y = _pPackets[g_lXContexts[0].iY];
		ULONG width = _pPackets[g_lXContexts[0].iWidth];
		ULONG height = _pPackets[g_lXContexts[0].iHeight];

		std::ostringstream args_stream;
		args_stream << x * ScaleX << "," << y * ScaleY;

		g_Channel->InvokeMethod("StylusDown", std::make_unique<flutter::EncodableValue>(flutter::EncodableValue(args_stream.str())));
		return S_OK;
	}
	STDMETHOD(StylusUp)(IRealTimeStylus*, const StylusInfo*, ULONG, LONG* _pPackets, LONG**) {
		g_Channel->InvokeMethod("StylusUp", nullptr);
		return S_OK;
	}
	STDMETHOD(RealTimeStylusEnabled)(IRealTimeStylus*, ULONG, const TABLET_CONTEXT_ID*) {
		g_Channel->InvokeMethod("RealTimeStylusEnabled", nullptr);
		return S_OK;
	}
	STDMETHOD(RealTimeStylusDisabled)(IRealTimeStylus*, ULONG, const TABLET_CONTEXT_ID*) {
		g_Channel->InvokeMethod("RealTimeStylusDisabled", nullptr);
		return S_OK;
	}
	STDMETHOD(StylusInRange)(IRealTimeStylus*, TABLET_CONTEXT_ID, STYLUS_ID) {
		g_Channel->InvokeMethod("StylusInRange", nullptr);
		return S_OK;
	}
	STDMETHOD(StylusOutOfRange)(IRealTimeStylus*, TABLET_CONTEXT_ID, STYLUS_ID) {
		g_Channel->InvokeMethod("StylusOutOfRange", nullptr);
		return S_OK;
	}
	STDMETHOD(InAirPackets)(IRealTimeStylus*, const StylusInfo*, ULONG, ULONG, LONG*, ULONG*, LONG**) {
		g_Channel->InvokeMethod("InAirPackets", nullptr);
		return S_OK;
	}
	STDMETHOD(StylusButtonUp)(IRealTimeStylus*, STYLUS_ID, const GUID*, POINT*) {
		g_Channel->InvokeMethod("StylusButtonUp", nullptr);
		return S_OK;
	}
	STDMETHOD(StylusButtonDown)(IRealTimeStylus*, STYLUS_ID, const GUID*, POINT*) {
		g_Channel->InvokeMethod("StylusButtonDown", nullptr);
		return S_OK;
	}
	STDMETHOD(SystemEvent)(IRealTimeStylus*, TABLET_CONTEXT_ID, STYLUS_ID, SYSTEM_EVENT, SYSTEM_EVENT_DATA) {
		g_Channel->InvokeMethod("IRealTimeStylus", nullptr);
		return S_OK;
	}
	STDMETHOD(TabletAdded)(IRealTimeStylus*, IInkTablet*) {
		g_Channel->InvokeMethod("TabletAdded", nullptr);
		return S_OK;
	}
	STDMETHOD(TabletRemoved)(IRealTimeStylus*, LONG) {
		g_Channel->InvokeMethod("TabletRemoved", nullptr);
		return S_OK;
	}
	STDMETHOD(CustomStylusDataAdded)(IRealTimeStylus*, const GUID*, ULONG, const BYTE*) {
		g_Channel->InvokeMethod("CustomStylusDataAdded", nullptr);
		return S_OK;
	}
	STDMETHOD(Error)(IRealTimeStylus*, IStylusPlugin*, RealTimeStylusDataInterest, HRESULT, LONG_PTR*) {
		g_Trace += 2;
		g_Channel->InvokeMethod("Error", nullptr);
		return S_OK;
	}
	STDMETHOD(UpdateMapping)(IRealTimeStylus*) {
		g_Channel->InvokeMethod("UpdateMapping", nullptr);
		return S_OK;
	}

	// IUnknown methods
	STDMETHOD_(ULONG, AddRef)()
	{
		g_Channel->InvokeMethod("AddRef", nullptr);
		return InterlockedIncrement(&m_nRef);
	}
	STDMETHOD_(ULONG, Release)()
	{
		g_Channel->InvokeMethod("Release", nullptr);
		ULONG nNewRef = InterlockedDecrement(&m_nRef);
		if (nNewRef == 0)
			delete this;

		return nNewRef;
	}
	STDMETHOD(QueryInterface)(REFIID riid, LPVOID* ppvObj)
	{
		g_Channel->InvokeMethod("QueryInterface", nullptr);
		if ((riid == IID_IStylusSyncPlugin) || (riid == IID_IUnknown))
		{
			*ppvObj = this;
			AddRef();
			return S_OK;
		}
		else if ((riid == IID_IMarshal) && (m_pPunkFTMarshaller != NULL))
		{
			return m_pPunkFTMarshaller->QueryInterface(riid, ppvObj);
		}

		*ppvObj = NULL;
		return E_NOINTERFACE;
	}

	LONG m_nRef;					// COM object reference count
	IUnknown* m_pPunkFTMarshaller;  // free-threaded marshaller
};

namespace {

	//-------------------------------------------------------------------------------------------------
	void ReleaseRTS()
	{
		g_Channel->InvokeMethod("ReleaseRTS", nullptr);
		if (g_pRTSStylus)
		{
			g_pRTSStylus->Release();
			g_pRTSStylus = NULL;
		}

		if (g_pRTSHandler)
		{
			g_pRTSHandler->Release();
			g_pRTSHandler = NULL;
		}
	}


	//-------------------------------------------------------------------------------------------------
	// Create stylus
	bool CreateRTS(HWND hWnd)
	{
		g_Channel->InvokeMethod("CreateRTS", nullptr);
		// Release, just in case
		ReleaseRTS();

		// Create stylus
		HRESULT hr = CoCreateInstance(CLSID_RealTimeStylus, NULL, CLSCTX_ALL, IID_PPV_ARGS(&g_pRTSStylus));
		if (FAILED(hr))
			return false;

		// Attach RTS object to a window
		hr = g_pRTSStylus->put_HWND((HANDLE_PTR)hWnd);
		if (FAILED(hr))
		{
			ReleaseRTS();
			return false;
		}

		// Create eventhandler
		g_pRTSHandler = new CRTSEventHandler();

		// Create free-threaded marshaller for this object and aggregate it.
		hr = CoCreateFreeThreadedMarshaler(g_pRTSHandler, &g_pRTSHandler->m_pPunkFTMarshaller);
		if (FAILED(hr))
		{
			ReleaseRTS();
			return false;
		}

		// Add handler object to the list of synchronous plugins in the RTS object.
		hr = g_pRTSStylus->AddStylusSyncPlugin(0, g_pRTSHandler);
		if (FAILED(hr))
		{
			ReleaseRTS();
			return false;
		}

		// Set data we want - we're not actually using all of this, but we're gonna get X and Y anyway so might as well set it
		GUID lWantedProps[] = {
			GUID_PACKETPROPERTY_GUID_X,
			GUID_PACKETPROPERTY_GUID_Y,
			GUID_PACKETPROPERTY_GUID_NORMAL_PRESSURE,
			GUID_PACKETPROPERTY_GUID_WIDTH,
			GUID_PACKETPROPERTY_GUID_HEIGHT
		};
		g_pRTSStylus->SetDesiredPacketDescription(5, lWantedProps);
		g_pRTSStylus->put_Enabled(true);

		// Detect what tablet context IDs will give us pressure data
		{
			g_nContexts = 0;
			ULONG nTabletContexts = 0;
			TABLET_CONTEXT_ID* piTabletContexts;
			HRESULT res = g_pRTSStylus->GetAllTabletContextIds(&nTabletContexts, &piTabletContexts);
			for (ULONG i = 0; i < nTabletContexts; i++)
			{
				IInkTablet* pInkTablet;
				if (SUCCEEDED(g_pRTSStylus->GetTabletFromTabletContextId(piTabletContexts[i], &pInkTablet)))
				{
					// float ScaleX, ScaleY;
					ULONG nPacketProps;
					PACKET_PROPERTY* pPacketProps;
					if (i == 0) {
						g_pRTSStylus->GetPacketDescriptionData(piTabletContexts[i], &ScaleX, &ScaleY, &nPacketProps, &pPacketProps);
					}
					else {
						float a;
						float b;
						g_pRTSStylus->GetPacketDescriptionData(piTabletContexts[i], &a, &b, &nPacketProps, &pPacketProps);
					}

					for (ULONG j = 0; j < nPacketProps; j++)
					{
						if (pPacketProps[j].guid == GUID_PACKETPROPERTY_GUID_X) {
							g_lXContexts[0].iTabletContext = piTabletContexts[i];
							g_lXContexts[0].iX = j;
						}
						if (pPacketProps[j].guid == GUID_PACKETPROPERTY_GUID_Y) {
							g_lXContexts[0].iY = j;
						}
						if (pPacketProps[j].guid == GUID_PACKETPROPERTY_GUID_WIDTH) {
							g_lXContexts[0].iWidth = j;
						}
						if (pPacketProps[j].guid == GUID_PACKETPROPERTY_GUID_HEIGHT) {
							g_lXContexts[0].iHeight = j;
						}

						if (pPacketProps[j].guid != GUID_PACKETPROPERTY_GUID_NORMAL_PRESSURE)
							continue;

						g_lContexts[g_nContexts].iTabletContext = piTabletContexts[i];
						g_lContexts[g_nContexts].iPressure = j;
						g_lContexts[g_nContexts].PressureRcp = 1.0f / pPacketProps[j].PropertyMetrics.nLogicalMax;
						g_nContexts++;
						break;
					}
					CoTaskMemFree(pPacketProps);
				}
			}

			// If we can't get pressure information, no use in having the tablet context
			if (g_nContexts == 0)
			{
				ReleaseRTS();
				return false;
			}
		}

		return true;
	}


	class StylusApiPlugin : public flutter::Plugin {
	public:
		static void RegisterWithRegistrar(flutter::PluginRegistrarWindows* registrar);

		StylusApiPlugin();

		virtual ~StylusApiPlugin();

	private:
		// Called when a method is called on this plugin's channel from Dart.
		void HandleMethodCall(
			const flutter::MethodCall<flutter::EncodableValue>& method_call,
			std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result);
	};

	// static
	void StylusApiPlugin::RegisterWithRegistrar(
		flutter::PluginRegistrarWindows* registrar) {

		auto channel =
			std::shared_ptr<flutter::MethodChannel<flutter::EncodableValue>>(new flutter::MethodChannel<flutter::EncodableValue>(
				registrar->messenger(), "stylus_api",
				&flutter::StandardMethodCodec::GetInstance()));

		g_Channel = channel;
		g_NativeWindows = registrar->GetView()->GetNativeWindow();
		auto plugin = std::make_unique<StylusApiPlugin>();

		channel->SetMethodCallHandler(
			[plugin_pointer = plugin.get()](const auto& call, auto result) {
			plugin_pointer->HandleMethodCall(call, std::move(result));
		});

		registrar->AddPlugin(std::move(plugin));
	}

	StylusApiPlugin::StylusApiPlugin() {}

	StylusApiPlugin::~StylusApiPlugin() {}

	void StylusApiPlugin::HandleMethodCall(
		const flutter::MethodCall<flutter::EncodableValue>& method_call,
		std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result) {
		if (method_call.method_name().compare("getPlatformVersion") == 0) {
			std::ostringstream version_stream;
			version_stream << "Windows ";
			if (IsWindows10OrGreater()) {
				version_stream << "10+";
			}
			else if (IsWindows8OrGreater()) {
				version_stream << "8";
			}
			else if (IsWindows7OrGreater()) {
				version_stream << "7";
			}
			result->Success(flutter::EncodableValue(version_stream.str()));
		}
		else if (method_call.method_name().compare("createRTS") == 0) {
			// HWND handle = GetActiveWindow();
			bool success = CreateRTS(g_NativeWindows);
			result->Success(flutter::EncodableValue(success));
		}
		else if (method_call.method_name().compare("releaseRTS") == 0) {
			ReleaseRTS();
			result->Success(nullptr);
		}
		else {
			result->NotImplemented();
		}
	}

}  // namespace

void StylusApiPluginRegisterWithRegistrar(
	FlutterDesktopPluginRegistrarRef registrar) {
	StylusApiPlugin::RegisterWithRegistrar(
		flutter::PluginRegistrarManager::GetInstance()
		->GetRegistrar<flutter::PluginRegistrarWindows>(registrar));
}

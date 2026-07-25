#pragma once

#ifdef _WIN32
#include <windows.h>
#include <unknwn.h>

#include <cstdint>

enum class DXCoreAdapterProperty : uint32_t {
    InstanceLuid = 0,
    DedicatedAdapterMemory = 7,
    AdapterEngineCount = 16
};

enum class DXCoreAdapterState : uint32_t {
    AdapterMemoryBudget = 1,
    AdapterMemoryUsageBytes = 2,
    AdapterMemoryUsageByProcessBytes = 3,
    AdapterEngineRunningTimeByProcessMicroseconds = 5,
    AdapterTemperatureCelsius = 6,
    AdapterInUseProcessCount = 7,
    AdapterInUseProcessSet = 8,
    AdapterEngineFrequencyHertz = 9,
    AdapterMemoryFrequencyHertz = 10
};

enum class DXCoreSegmentGroup : uint32_t {
    Local = 0,
    NonLocal = 1
};

enum class DXCoreMemoryType : uint32_t {
    Dedicated = 0,
    Shared = 1
};

struct DXCoreMemoryUsage {
    uint64_t committed;
    uint64_t resident;
};

struct DXCoreMemoryQueryInput {
    uint32_t physicalAdapterIndex;
    DXCoreMemoryType memoryType;
};

struct DXCoreFrequencyQueryOutput {
    uint64_t frequency;
    uint64_t maxFrequency;
    uint64_t maxOverclockedFrequency;
};

struct DXCoreAdapterEngineIndex {
    uint32_t physicalAdapterIndex;
    uint32_t engineIndex;
};

struct DXCoreEngineQueryInput {
    DXCoreAdapterEngineIndex adapterEngineIndex;
    uint32_t processId;
};

struct DXCoreEngineQueryOutput {
    uint64_t runningTime;
    bool processQuerySucceeded;
};

struct DXCoreProcessMemoryQueryInput {
    uint32_t physicalAdapterIndex;
    DXCoreMemoryType memoryType;
    uint32_t processId;
};

struct DXCoreProcessMemoryQueryOutput {
    DXCoreMemoryUsage memoryUsage;
    bool processQuerySucceeded;
};

struct DXCoreAdapterProcessSetQueryInput {
    uint32_t arraySize;
    uint32_t *processIds;
};

struct DXCoreAdapterProcessSetQueryOutput {
    uint32_t processesWritten;
    uint32_t processesTotal;
};

static_assert(sizeof(bool) == 1, "DXCore bool size mismatch");

struct DXCoreAdapterMemoryBudgetNodeSegmentGroup {
    uint32_t nodeIndex;
    DXCoreSegmentGroup segmentGroup;
};

struct DXCoreAdapterMemoryBudget {
    uint64_t budget;
    uint64_t currentUsage;
    uint64_t availableForReservation;
    uint64_t currentReservation;
};

inline const GUID IID_IDXCoreAdapterFactory = {0x78ee5945, 0xc36e, 0x4b13, {0xa6, 0x69, 0x00, 0x5d, 0xd1, 0x1c, 0x0f, 0x06}};
inline const GUID IID_IDXCoreAdapter = {0xf0db4c7f, 0xfe5a, 0x42a2, {0xbd, 0x62, 0xf2, 0xa6, 0xcf, 0x6f, 0xc8, 0x3e}};

struct IDXCoreAdapter : public IUnknown {
    virtual bool STDMETHODCALLTYPE IsValid() = 0;
    virtual bool STDMETHODCALLTYPE IsAttributeSupported(REFGUID attribute) = 0;
    virtual bool STDMETHODCALLTYPE IsPropertySupported(DXCoreAdapterProperty property) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetProperty(DXCoreAdapterProperty property, size_t bufferSize, void *propertyData) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetPropertySize(DXCoreAdapterProperty property, size_t *bufferSize) = 0;
    virtual bool STDMETHODCALLTYPE IsQueryStateSupported(DXCoreAdapterState property) = 0;
    virtual HRESULT STDMETHODCALLTYPE QueryState(DXCoreAdapterState state,
        size_t inputStateDetailsSize, const void *inputStateDetails,
        size_t outputBufferSize, void *outputBuffer) = 0;
    virtual bool STDMETHODCALLTYPE IsSetStateSupported(DXCoreAdapterState property) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetState(DXCoreAdapterState state,
        size_t inputStateDetailsSize, const void *inputStateDetails,
        size_t inputDataSize, const void *inputData) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetFactory(REFIID riid, void **ppvFactory) = 0;
};

struct IDXCoreAdapterFactory : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE CreateAdapterList(uint32_t numAttributes,
        const GUID *filterAttributes, REFIID riid, void **ppvAdapterList) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetAdapterByLuid(const LUID &adapterLUID,
        REFIID riid, void **ppvAdapter) = 0;
    virtual bool STDMETHODCALLTYPE IsNotificationTypeSupported(uint32_t notificationType) = 0;
    virtual HRESULT STDMETHODCALLTYPE RegisterEventNotification(IUnknown *dxCoreObject,
        uint32_t notificationType, void *callbackFunction, void *callbackContext,
        uint32_t *eventCookie) = 0;
    virtual HRESULT STDMETHODCALLTYPE UnregisterEventNotification(uint32_t eventCookie) = 0;
};

using PFN_DXCORE_CREATE_ADAPTER_FACTORY = HRESULT (WINAPI *)(REFIID riid, void **ppvFactory);

#endif

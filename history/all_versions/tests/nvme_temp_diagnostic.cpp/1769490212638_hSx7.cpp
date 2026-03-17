// nvme_temp_diagnostic.cpp - Debug tool for NVMe temperature queries
#include <windows.h>
#include <stdio.h>
#include <winioctl.h>

// Storage property definitions
typedef enum _STORAGE_PROPERTY_ID {
    StorageDeviceProperty = 0,
    StorageAdapterProperty = 1,
    StorageDeviceIdProperty = 2,
    StorageDeviceUniqueIdProperty = 3,
    StorageDeviceWriteCacheProperty = 4,
    StorageMiniportProperty = 5,
    StorageAccessAlignmentProperty = 6,
    StorageDeviceSeekPenaltyProperty = 7,
    StorageDeviceTrimProperty = 8,
    StorageDeviceWriteAggregationProperty = 9,
    StorageDeviceDeviceTelemetryProperty = 10,
    StorageDeviceLBProvisioningProperty = 11,
    StorageDevicePowerProperty = 12,
    StorageDeviceCopyOffloadProperty = 13,
    StorageDeviceResiliencyProperty = 14,
    StorageDeviceMediumProductType = 15,
    StorageAdapterRpmbProperty = 16,
    StorageAdapterCryptoProperty = 17,
    StorageDeviceIoCapabilityProperty = 48,
    StorageAdapterProtocolSpecificProperty = 49,
    StorageDeviceProtocolSpecificProperty = 50,
    StorageAdapterTemperatureProperty = 51,
    StorageDeviceTemperatureProperty = 52,
    StorageAdapterPhysicalTopologyProperty = 53,
    StorageDevicePhysicalTopologyProperty = 54,
    StorageDeviceAttributesProperty = 55,
    StorageDeviceManagementStatus = 56,
    StorageAdapterSerialNumberProperty = 57,
    StorageDeviceLocationProperty = 58,
    StorageDeviceNumaProperty = 59,
    StorageDeviceZonedDeviceProperty = 60,
    StorageDeviceUnsafeShutdownCount = 61,
    StorageDeviceEnduranceProperty = 62,
} STORAGE_PROPERTY_ID;

typedef enum _STORAGE_QUERY_TYPE {
    PropertyStandardQuery = 0,
    PropertyExistsQuery = 1,
    PropertyMaskQuery = 2,
    PropertyQueryMaxDefined = 3,
} STORAGE_QUERY_TYPE;

typedef struct _STORAGE_PROPERTY_QUERY {
    STORAGE_PROPERTY_ID PropertyId;
    STORAGE_QUERY_TYPE QueryType;
    BYTE AdditionalParameters[1];
} STORAGE_PROPERTY_QUERY;

typedef struct _STORAGE_TEMPERATURE_INFO {
    WORD Index;
    SHORT Temperature;
    SHORT OverThreshold;
    SHORT UnderThreshold;
    BOOLEAN OverThresholdChangable;
    BOOLEAN UnderThresholdChangable;
    BOOLEAN EventGenerated;
    BYTE Reserved0;
    DWORD Reserved1;
} STORAGE_TEMPERATURE_INFO;

typedef struct _STORAGE_TEMPERATURE_DATA_DESCRIPTOR {
    DWORD Version;
    DWORD Size;
    SHORT CriticalTemperature;
    SHORT WarningTemperature;
    WORD InfoCount;
    BYTE Reserved0[2];
    STORAGE_TEMPERATURE_INFO TemperatureInfo[1];
} STORAGE_TEMPERATURE_DATA_DESCRIPTOR;

#define IOCTL_STORAGE_QUERY_PROPERTY CTL_CODE(IOCTL_STORAGE_BASE, 0x0500, METHOD_BUFFERED, FILE_ANY_ACCESS)

int main() {
    printf("=== SOVEREIGN NVMe Temperature Diagnostic ===\n\n");
    
    for (int driveId = 0; driveId < 6; driveId++) {
        char drivePath[32];
        sprintf_s(drivePath, "\\\\.\\PhysicalDrive%d", driveId);
        
        printf("Drive %d: %s\n", driveId, drivePath);
        
        HANDLE hDrive = CreateFileA(
            drivePath,
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL,
            OPEN_EXISTING,
            0,
            NULL
        );
        
        if (hDrive == INVALID_HANDLE_VALUE) {
            DWORD err = GetLastError();
            printf("  [FAIL] CreateFile failed: %lu", err);
            if (err == 5) printf(" (ACCESS_DENIED - run as admin)");
            else if (err == 2) printf(" (FILE_NOT_FOUND - drive doesn't exist)");
            printf("\n\n");
            continue;
        }
        
        printf("  [OK] Handle opened\n");
        
        // Query temperature property
        STORAGE_PROPERTY_QUERY query = {0};
        query.PropertyId = StorageDeviceTemperatureProperty;
        query.QueryType = PropertyStandardQuery;
        
        BYTE buffer[512] = {0};
        DWORD bytesReturned = 0;
        
        BOOL result = DeviceIoControl(
            hDrive,
            IOCTL_STORAGE_QUERY_PROPERTY,
            &query,
            sizeof(query),
            buffer,
            sizeof(buffer),
            &bytesReturned,
            NULL
        );
        
        if (!result) {
            DWORD err = GetLastError();
            printf("  [FAIL] DeviceIoControl failed: %lu", err);
            if (err == 1) printf(" (INVALID_FUNCTION - property not supported)");
            else if (err == 50) printf(" (NOT_SUPPORTED - device doesn't support temp query)");
            else if (err == 87) printf(" (INVALID_PARAMETER)");
            printf("\n");
        } else {
            printf("  [OK] DeviceIoControl succeeded, %lu bytes returned\n", bytesReturned);
            
            STORAGE_TEMPERATURE_DATA_DESCRIPTOR* pDesc = (STORAGE_TEMPERATURE_DATA_DESCRIPTOR*)buffer;
            printf("  Version: %lu\n", pDesc->Version);
            printf("  Size: %lu\n", pDesc->Size);
            printf("  Critical Temp: %d C\n", pDesc->CriticalTemperature);
            printf("  Warning Temp: %d C\n", pDesc->WarningTemperature);
            printf("  InfoCount: %u\n", pDesc->InfoCount);
            
            for (WORD i = 0; i < pDesc->InfoCount && i < 4; i++) {
                printf("  Sensor[%u]: Index=%u, Temp=%d C\n", 
                    i, 
                    pDesc->TemperatureInfo[i].Index,
                    pDesc->TemperatureInfo[i].Temperature);
            }
        }
        
        CloseHandle(hDrive);
        printf("\n");
    }
    
    printf("Press Enter to exit...\n");
    getchar();
    return 0;
}

#include "pcomp.h"
#include "mp_dev.h"

#pragma NDIS_PAGEABLE_FUNCTION(MPInitialize)
#pragma NDIS_PAGEABLE_FUNCTION(mpHalt)


#define LOG_D(fmt,...) //DbgPrint("[%s,%d] "fmt"\n",__FUNCTION__,__LINE__,##__VA_ARGS__);
#define LOG_N(fmt,...) DbgPrint("[%s,%d] "fmt"\n",__FUNCTION__,__LINE__,##__VA_ARGS__);
#define LOG_I(fmt,...) DbgPrint("[%s,%d] "fmt"\n",__FUNCTION__,__LINE__,##__VA_ARGS__);
#define LOG_W(fmt,...) DbgPrint("[%s,%d] "fmt"\n",__FUNCTION__,__LINE__,##__VA_ARGS__);
#define LOG_E(fmt,...) DbgPrint("[%s,%d] "fmt"\n",__FUNCTION__,__LINE__,##__VA_ARGS__);

#define NIC_DRIVER_NAME                 "SCI Inc. 9086 Wireless LAN Adapter Driver"

#ifdef NDIS51_MINIPORT
static wf_u8 mp_ndis_major = 5;
static wf_u8 mp_ndis_minor = 1;
#else
static unsigned char mp_ndis_major = 5;
static unsigned char mp_ndis_minor = 0;
#endif

VOID mpHalt(NDIS_HANDLE miniportAdapterContext) 
{
	P_GLUE_INFO_T prGlueInfo;
	PADAPTER prAdapter;
	prGlueInfo = (P_GLUE_INFO_T)miniportAdapterContext;
	prAdapter = prGlueInfo->prAdapter;
	LOG_D("Start to halt this dirver.");
	if(prAdapter){
		wf_dbg_deinit(prAdapter);

		wf_nic_dev_deinit(prAdapter);

		wf_usb_dev_stop(prAdapter);

		wf_usb_dev_deinit(prAdapter);
	}
	
	MpFreeGlue(prGlueInfo);
}

VOID mpHandleInterrupt(NDIS_HANDLE miniportAdapterContext) 
{
	LOG_D("enter");
}

VOID
mpIsr (
    OUT PBOOLEAN    interruptRecognized_p,
    OUT PBOOLEAN    queueMiniportHandleInterrupt_p,
    IN  NDIS_HANDLE miniportAdapterContext
    )
{
	LOG_D("enter");
}

NDIS_STATUS
mpQueryInformation (
	IN	NDIS_HANDLE miniportAdapterContext,
	IN	NDIS_OID	oid,
	IN	void	*pvInfomationBuffer,
	IN	ULONG 	u4InformationBufferLength,
	OUT ULONG	*pu4ByteWritten,
	OUT ULONG	*pu4ByteNeeded
	)
{
	P_GLUE_INFO_T       prGlueInfo = (P_GLUE_INFO_T)miniportAdapterContext;
	NDIS_STATUS         status;
	WLAN_REQ_ENTRY 		*entry = NULL;
	wf_u32              u4QueryInfoLen = u4InformationBufferLength;
	PADAPTER			prAdapter = prGlueInfo->prAdapter;
	//LOG_D("enter query");


	if(wf_req_search_supported_Oid_entry(oid, &entry) == FALSE) {
		LOG_E("can't find QUERY oid service! oid=%x", oid);
		return NDIS_STATUS_NOT_SUPPORTED;
	}

	if(entry == NULL || entry->pfOidQueryHandler == NULL) {
		LOG_E("entry or function is NULL! oid=%d", oid);
		return NDIS_STATUS_NOT_ACCEPTED;
	}

	if (entry->fgQryBufLenChecking) {
        if (u4InformationBufferLength < entry->u4InfoBufLen) {
            /* Not enough room in InformationBuffer. Punt */
            *pu4ByteNeeded = entry->u4InfoBufLen;

            LOG_E("Query %s: Buffer too short (current=%d, needed=%d)",
                entry->pucOidName, u4InformationBufferLength, *pu4ByteNeeded);
            return NDIS_STATUS_INVALID_LENGTH;
        }
    }

	LOG_D("oid=%s", entry->pucOidName);
	//prAdapter->PendedRequest = oid;

	switch(entry->eOidMethod) {
	case ENUM_OID_GLUE_ONLY:
		status = entry->pfOidQueryHandler(
					prGlueInfo,
					pvInfomationBuffer,
					u4InformationBufferLength,
					&u4QueryInfoLen);
		break;
	case ENUM_OID_GLUE_EXTENSION:
	case ENUM_OID_DRIVER_CORE:
		status = entry->pfOidQueryHandler(
					prGlueInfo->prAdapter,
		            pvInfomationBuffer,
		            u4InformationBufferLength,
		            &u4QueryInfoLen);
		break;
	default:
		LOG_E("oid method error!");
		break;
	}

	*pu4ByteNeeded = u4QueryInfoLen;
	if(status == NDIS_STATUS_SUCCESS) {
		if (u4QueryInfoLen > u4InformationBufferLength) {
                     /* This case is added to solve the problem of both 32-bit
                        and 64-bit counters supported for the general
                        statistics OIDs. */
             *pu4ByteWritten = u4InformationBufferLength;
         }
         else {
             *pu4ByteWritten = u4QueryInfoLen;
         }
	}
	if (status != NDIS_STATUS_PENDING)
	{
		// Request has completed
		//prAdapter->PendedRequest = NULL;
	}
	else
	{
		LOG_D("Request has been pended. Will complete asynchronously");
	}

	return status;
}

NDIS_STATUS
mpReset(
	OUT PBOOLEAN	addressingReset_p,
	IN	NDIS_HANDLE miniportAdapterContext
	)
{
	P_GLUE_INFO_T       prGlueInfo = (P_GLUE_INFO_T)miniportAdapterContext;
	PADAPTER	prAdapter;
	
	LOG_D("enter reset");
	prAdapter = prGlueInfo->prAdapter;
	if(prAdapter) wf_attr_reset(prAdapter);

	return NDIS_STATUS_SUCCESS;
}

NDIS_STATUS
mpSetInformation (
	IN	NDIS_HANDLE miniportAdapterContext,
	IN	NDIS_OID	oid,
	IN	PVOID		pvInfomationBuffer,
	IN	ULONG 	u4InformationBufferLength,
	OUT ULONG	*pu4ByteRead,
	OUT ULONG	*pu4ByteNeeded
	)
{
	P_GLUE_INFO_T       prGlueInfo = (P_GLUE_INFO_T)miniportAdapterContext;
	NDIS_STATUS         status;
	WLAN_REQ_ENTRY 		*entry = NULL;
	wf_u32              u4SetInfoLen;
	PADAPTER			prAdapter = prGlueInfo->prAdapter;

	*pu4ByteRead = 0;
    *pu4ByteNeeded = 0;

	if(wf_req_search_supported_Oid_entry(oid, &entry) == FALSE) {
		LOG_E("can't find SET oid service! oid=%x", oid);
		return NDIS_STATUS_NOT_SUPPORTED;
	}

	if(entry == NULL || entry->pfOidSetHandler == NULL) {
		LOG_E("entry or function is NULL! oid=%d", oid);
		return NDIS_STATUS_INVALID_OID;;
	}

	if (entry->fgQryBufLenChecking) {
        if (u4InformationBufferLength < entry->u4InfoBufLen) {
            /* Not enough room in InformationBuffer. Punt */
            *pu4ByteNeeded = entry->u4InfoBufLen;

            LOG_E("Query %s: Buffer too short (current=%d, needed=%d)",
                entry->pucOidName, u4InformationBufferLength, *pu4ByteNeeded);
            return NDIS_STATUS_INVALID_LENGTH;
        }
    }

	LOG_D("oid=%s", entry->pucOidName);
	//prAdapter->PendedRequest = oid;

	switch(entry->eOidMethod) {
	case ENUM_OID_GLUE_ONLY:
		status = entry->pfOidSetHandler(
					prGlueInfo,
					pvInfomationBuffer,
					u4InformationBufferLength,
					&u4SetInfoLen);
		break;
	case ENUM_OID_GLUE_EXTENSION:
	case ENUM_OID_DRIVER_CORE:
		status = entry->pfOidSetHandler(
					prGlueInfo->prAdapter,
		            pvInfomationBuffer,
		            u4InformationBufferLength,
		            &u4SetInfoLen);
		break;
	default:
		LOG_E("oid method error!");
		break;
	}

	switch (status) {
    case NDIS_STATUS_SUCCESS:
         *pu4ByteRead = u4SetInfoLen;
         break;

    case NDIS_STATUS_INVALID_LENGTH:
         *pu4ByteNeeded = u4SetInfoLen;

         LOG_D("Set %s: Invalid length (current=%d, needed=%d)",
             entry->pucOidName, u4InformationBufferLength, *pu4ByteNeeded);
         break;
	}
	if (status != NDIS_STATUS_PENDING)
	{
		// Request has completed
		//prAdapter->PendedRequest = NULL;
	}
	else
	{
		LOG_D("Request has been pended. Will complete asynchronously");
	}

	return status;
}

VOID
mpReturnPacket (
	IN NDIS_HANDLE	miniportAdapterContext,
	IN PNDIS_PACKET prPacket
	)
{
	LOG_D("enter return packets");
}



VOID mpShutdown(PVOID shutdownContext)
{
	LOG_D("enter");
}   /* mpShutdown */



/*----------------------------------------------------------------------------*/
/*!
* \brief This function allocates GLUE_INFO_T memory and its initialized
*        procedure.
*
* \param[in] rMiniportAdapterHandle Windows provided adapter handle
* \param[in] u2NdisVersion      16-bits value with major[15:8].minor[7:0] format
*                               for NDIS version expression
*
* \return Virtual address of allocated GLUE_INFO_T strucutre
*/
/*----------------------------------------------------------------------------*/
P_GLUE_INFO_T
windowsCreateGlue (
    IN NDIS_HANDLE rMiniportAdapterHandle,
    IN wf_u16     u2NdisVersion
    )
{
    NDIS_STATUS status;
    P_GLUE_INFO_T prGlueInfo;

    LOG_D("windowsCreateGlue");

    /* Setup private information of glue layer for this adapter */
    status = NdisAllocateMemoryWithTag((PVOID*)&prGlueInfo,
                sizeof(GLUE_INFO_T), MP_MEMORY_TAG);
    if (status != NDIS_STATUS_SUCCESS) {
        return NULL;
    }

    NdisZeroMemory(prGlueInfo, sizeof(GLUE_INFO_T));

    /* set miniport handler supplied by OS */
    prGlueInfo->rMiniportAdapterHandle = rMiniportAdapterHandle;

    NdisInterlockedIncrement(&prGlueInfo->exitRefCount);

    prGlueInfo->u2NdisVersion = u2NdisVersion;

    return prGlueInfo;
} /* windowsCreateGlue */



/*----------------------------------------------------------------------------*/
/*!
* \brief This function is called to read Windows registry parameters
*
* \param prGlueInfo[in] 					Pointer to GLUE structure
* \param wrapperConfigurationContext[in]	Specifies a handle used only in
*				MiniportInitialize() for calls to NdisXXX configuration and
*				initialization functions.
*
* \retval
*/
/*----------------------------------------------------------------------------*/
NDIS_STATUS
windowsReadRegistryParameters(
	IN P_GLUE_INFO_T	prGlueInfo,
	IN NDIS_HANDLE		wrapperConfigurationContext
	)
{
#if 0
#define REG_NAME_MAX_LEN    32

	NDIS_STATUS 				rStatus = NDIS_STATUS_SUCCESS;
	NDIS_HANDLE 				configurationHandle;
	P_WLAN_REG_ENTRY_T			prRegEntry;
	UINT						i;
	PNDIS_CONFIGURATION_PARAMETER returnedValue_p;
	UINT						j;
	wf_u8						ansiRegName[REG_NAME_MAX_LEN];
	wf_u16 					u2StrLen;
	wf_u8 					*pucNetworkAddress;
	wf_u32 					u4NetworkAddrLeng;
	wf_u32					*pc;

	LOG_D("windowsReadRegistryParameters");

	/* Open the registry for this adapter */
	NdisOpenConfiguration(&rStatus, &configurationHandle,
			wrapperConfigurationContext);

	if (rStatus != NDIS_STATUS_SUCCESS) {
		LOG_E("NdisOpenConfiguration failed (status = 0x%x)", rStatus);
		return rStatus;
	}

	/* Read NetworkAddress registry value */
	/* Use it as the current address if any */
	NdisReadNetworkAddress(&rStatus,
		(PVOID)&pucNetworkAddress,
		&u4NetworkAddrLeng,
		configurationHandle);

	/* If there is a NetworkAddress override in registry, use it */
	if (rStatus == NDIS_STATUS_SUCCESS && u4NetworkAddrLeng == PARAM_MAC_ADDR_LEN) {
		LOG_D("NetworkAddress			 %02x-%02x-%02x-%02x-%02x-%02x",
			pucNetworkAddress[0], pucNetworkAddress[1], pucNetworkAddress[2],
			pucNetworkAddress[3], pucNetworkAddress[4], pucNetworkAddress[5]);

		if (pucNetworkAddress[0] & BIT(0)) {
			LOG_W("Overriding NetworkAddress is invalid - %02x-%02x-%02x-%02x-%02x-%02x",
				pucNetworkAddress[0], pucNetworkAddress[1],
				pucNetworkAddress[2], pucNetworkAddress[3],
				pucNetworkAddress[4], pucNetworkAddress[5]);
			NdisZeroMemory((PVOID)((wf_u8 *)prGlueInfo+GLUE_GET_REG_OFFSET(aucMacAddr)),
					PARAM_MAC_ADDR_LEN);
		}
		else {
			kalMemCopy((PVOID)((wf_u8*)prGlueInfo+GLUE_GET_REG_OFFSET(aucMacAddr)),
					(PVOID)pucNetworkAddress, PARAM_MAC_ADDR_LEN);
		}
	}
	else {
		// set LSB to indicate it's a invalid address
		prGlueInfo->rRegInfo.aucMacAddr[0] = BIT(0);
	}

	rStatus = NDIS_STATUS_SUCCESS;

	/* Read all the registry values. */
	prRegEntry = arWlanRegTable;
	for (i = 0; i < NUM_REG_PARAMS; i++, prRegEntry++) {
		for (j = 0; j < (UINT) (prRegEntry->regName.Length / 2) &&
			 j < REG_NAME_MAX_LEN-1; j++) {
			ansiRegName[j] = (UINT8) prRegEntry->regName.Buffer[j];
		}
		ansiRegName[j] = '\0';

		/* Get the configuration value for a specific parameter.  Under NT the
		 * parameters are all read in as DWORDs.
		 */
		NdisReadConfiguration(&rStatus, &returnedValue_p, configurationHandle,
			&prRegEntry->regName, prRegEntry->ParameterType);

		/* If the parameter was present, then check its value for validity. */
		if (rStatus == NDIS_STATUS_SUCCESS) {
			/* Check that param value is not too small or too large */
			switch (prRegEntry->ParameterType){
				case NdisParameterBinary:
					if (returnedValue_p->ParameterData.BinaryData.Length == 4) {
						if (prRegEntry->fieldSize == 4) {
							pc = (wf_u32*)returnedValue_p->ParameterData.BinaryData.Buffer;
							*(wf_u32*)((wf_u8*)prGlueInfo+prRegEntry->u4GlueOffset) = (wf_u32)pc[0];
							DBGLOG(INIT, TRACE, ("%-25s 0x%08x \n", ansiRegName,
							(wf_u32)*(wf_u32*)((wf_u8*)prGlueInfo+prRegEntry->u4GlueOffset)));
						}
						else {
							DBGLOG(INIT, ERROR, ("Driver Support only 4 bytes binary to formart as DWORD Hex value,"
								"Add extra binary format here!!\n"));
						}
					}
					else {
						if (prRegEntry->fieldSize == 4) {
							/* Here to read the field size of 4 for already released inf file
							   with DWORD value now using binary format, for BWCS-related registry */
							NdisReadConfiguration(&rStatus, &returnedValue_p, configurationHandle,
								&prRegEntry->regName, NdisParameterHexInteger);
							if (rStatus == NDIS_STATUS_SUCCESS) {
								if ((UINT) returnedValue_p->ParameterData.IntegerData <
									prRegEntry->min ||
									(UINT) returnedValue_p->ParameterData.IntegerData >
									prRegEntry->max) {
										*(wf_u32*)((wf_u8*)prGlueInfo + prRegEntry->u4GlueOffset) =
											prRegEntry->rDefaultValue.u4IntData;
								}
								else { /* Use the value if it is within range */
									*(wf_u32*)((wf_u8*)prGlueInfo+prRegEntry->u4GlueOffset) =
										 returnedValue_p->ParameterData.IntegerData;
								}
								DBGLOG(INIT, TRACE, ("%-25s 0x%08x\n", ansiRegName,
									*(wf_u32*)((wf_u8*)prGlueInfo+prRegEntry->u4GlueOffset)));
							}
						}
						else if (prRegEntry->fieldSize == 1) {
							*(PINT_8)((wf_u8*)prGlueInfo+prRegEntry->u4GlueOffset) =
								(*(PINT_8) (returnedValue_p->ParameterData.BinaryData.Buffer));

							DBGLOG(INIT, TRACE, ("%-25s 0x%08x\n", ansiRegName,
										*(PINT_8)((wf_u8*)prGlueInfo+prRegEntry->u4GlueOffset)));
						}
						else {
							DBGLOG(INIT, ERROR, ("Driver Support only 1/4 bytes binary to formart as DWORD Hex value,"
								"Add extra binary format here!!\n"));
						}
					}
					break;

				case NdisParameterHexInteger:
				case NdisParameterInteger:
					if ((UINT) returnedValue_p->ParameterData.IntegerData <
						prRegEntry->min ||
						(UINT) returnedValue_p->ParameterData.IntegerData >
						prRegEntry->max) {
							  *(wf_u32*)((wf_u8*)prGlueInfo + prRegEntry->u4GlueOffset) =
								prRegEntry->rDefaultValue.u4IntData;
					}
					else { /* Use the value if it is within range */
						*(wf_u32*)((wf_u8*)prGlueInfo+prRegEntry->u4GlueOffset) =
							 returnedValue_p->ParameterData.IntegerData;
					}
					DBGLOG(INIT, TRACE, ("%-25s 0x%08x\n", ansiRegName,
						*(wf_u32*)((wf_u8*)prGlueInfo+prRegEntry->u4GlueOffset)));
					break;

				case NdisParameterString:
				case NdisParameterMultiString:
					u2StrLen = returnedValue_p->ParameterData.StringData.Length;
					if (u2StrLen < prRegEntry->min || u2StrLen > prRegEntry->max) {
						DBGLOG(INIT, TRACE, ("Out of range: %d, min: %d, max:%d\n",
									u2StrLen,
									prRegEntry->min,
									prRegEntry->max) );
						kalMemCopy((wf_u8*)prGlueInfo + prRegEntry->u4GlueOffset,
									prRegEntry->rDefaultValue.pucStrData,
									prRegEntry->fieldSize);
					}
					else { /* Use the value if it is within range */
						wf_u8* pucTerm;

						kalMemCopy((wf_u8*)prGlueInfo + prRegEntry->u4GlueOffset,
								   (wf_u8*)&returnedValue_p->ParameterData.StringData.Buffer[0],
								   u2StrLen);

						pucTerm = (wf_u8*)prGlueInfo + prRegEntry->u4GlueOffset + u2StrLen;

						*(pucTerm) = '\0';
						*(pucTerm+1) = '\0';
					}
					DBGLOG(INIT, TRACE, ("%-25s :%ws\n", ansiRegName,\
						((wf_u8*)prGlueInfo+prRegEntry->u4GlueOffset) ));
					break;

				default:
					DBGLOG(INIT, ERROR, ("parameter type error %d\n",prRegEntry->ParameterType) );
					break;
			}

		}
		else if (prRegEntry->mandatory) {
			/* If a mandatory parameter wasn't present then error out. */
			DBGLOG(REQ, ERROR, ("Could not find mandantory '%s' in registry\n\n",
				ansiRegName));
			rStatus = NDIS_STATUS_FAILURE;
			break;
		}
		else {
			/* If a non-mandatory parameter wasn't present, then set it to its
			   default value. */
			switch (prRegEntry->ParameterType){
				case NdisParameterBinary:
				case NdisParameterHexInteger:
				case NdisParameterInteger:
					if (prRegEntry->fieldSize == 4) {
						*(wf_u32*)((wf_u8*)prGlueInfo+prRegEntry->u4GlueOffset) =
							prRegEntry->rDefaultValue.u4IntData;

						DBGLOG(INIT, TRACE, ("%-25s 0x%08x\n", ansiRegName,
									*(wf_u32*)((wf_u8*)prGlueInfo+prRegEntry->u4GlueOffset) ));
					}
					else if (prRegEntry->fieldSize == 1) {
						*(PINT_8)((wf_u8*)prGlueInfo+prRegEntry->u4GlueOffset) =
							(INT_8) (prRegEntry->rDefaultValue.u4IntData);

						DBGLOG(INIT, TRACE, ("%-25s 0x%08x\n", ansiRegName,
									*(PINT_8)((wf_u8*)prGlueInfo+prRegEntry->u4GlueOffset) ));
					}
					break;
				case NdisParameterString:
				case NdisParameterMultiString:
					if (prRegEntry->rDefaultValue.pucStrData) {
						memcpy((wf_u8*)prGlueInfo + prRegEntry->u4GlueOffset,
							   prRegEntry->rDefaultValue.pucStrData,
							   prRegEntry->fieldSize * sizeof(WCHAR));
						DBGLOG(INIT, TRACE, ("%-25s \"%ws\"\n", ansiRegName,\
							((wf_u8*)prGlueInfo+prRegEntry->u4GlueOffset) ));
					}
					 break;
				default:
					DBGLOG(INIT, ERROR, ("parameter type error %d\n",
							prRegEntry->ParameterType) );
					break;
			}

			rStatus = NDIS_STATUS_SUCCESS;
		}
	}

	/* Close the registry */
	NdisCloseConfiguration(configurationHandle);

	return rStatus;
#endif
	return 0;
}	
#if 0
NTSTATUS wf_usb_dev_create(PADAPTER adapter)
{
	WDF_USB_DEVICE_SELECT_CONFIG_PARAMS configParams;
	NTSTATUS                            ntStatus;
	WDFUSBPIPE                          pipe;
	WDF_USB_PIPE_INFORMATION            pipeInfo;
	UCHAR                               index;
	UCHAR                               numberConfiguredPipes;
	PUSB_DEVICE_CONTEXT                 usbDeviceContext;
	WDF_OBJECT_ATTRIBUTES               attributes;
	//WDFDEVICE                           WdfDevice;

	//
	// Create a USB device handle so that we can communicate with the
	// underlying USB stack. The WDFUSBDEVICE handle is used to query,
	// configure, and manage all aspects of the USB device.
	// These aspects include device properties, bus properties,
	// and I/O creation and synchronization.
	//
	WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, USB_DEVICE_CONTEXT);
	//WdfDevice = Mp11GetWdfDevice(adapter);
	ntStatus = WdfUsbTargetDeviceCreate(adapter->WdfDevice,
		&attributes,
		&adapter->UsbDevice);

	if (!NT_SUCCESS(ntStatus)) {
		LOG_E("WdfUsbTargetDeviceCreate failed with Status code %!STATUS!", ntStatus);
		return ntStatus;
	}

	WDF_USB_DEVICE_SELECT_CONFIG_PARAMS_INIT_SINGLE_INTERFACE(&configParams);
	ntStatus = WdfUsbTargetDeviceSelectConfig(adapter->UsbDevice,
		WDF_NO_OBJECT_ATTRIBUTES,
		&configParams);

	if (!NT_SUCCESS(ntStatus)) {
		LOG_E("WdfUsbTargetDeviceSelectConfig failed %!STATUS!", ntStatus);
		return ntStatus;
	}

	usbDeviceContext = GetUsbDeviceContext(adapter->UsbDevice);
	usbDeviceContext->UsbInterface =
		configParams.Types.SingleInterface.ConfiguredUsbInterface;
	numberConfiguredPipes = configParams.Types.SingleInterface.NumberConfiguredPipes;

	if (numberConfiguredPipes > MAX_PIPES) {
		numberConfiguredPipes = MAX_PIPES;
	}

	usbDeviceContext->NumberOfPipes = numberConfiguredPipes;
	//
	// If we didn't find the 3 pipes, fail the start.
	//

	if (numberConfiguredPipes < 3) {
		ntStatus = STATUS_INVALID_DEVICE_STATE;
		LOG_E("Device is not configured properly %!STATUS!", ntStatus);
		return ntStatus;

	}

	//
	// Get pipe handles
	//
	for (index = 0; index < numberConfiguredPipes; index++) {

		WDF_USB_PIPE_INFORMATION_INIT(&pipeInfo);

		pipe = WdfUsbInterfaceGetConfiguredPipe(usbDeviceContext->UsbInterface,
			index, //PipeIndex,
			&pipeInfo
		);
		//
		// Tell the framework that it's okay to read less than
		// MaximumPacketSize
		//
		WdfUsbTargetPipeSetNoMaximumPacketSizeCheck(pipe);

		if (WdfUsbPipeTypeBulk == pipeInfo.PipeType) {
			LOG_D("BulkInput Pipe is 0x%p direction : %s", pipe,
				WdfUsbTargetPipeIsInEndpoint(pipe) ? "__in" : "__out");

			if (WdfUsbTargetPipeIsInEndpoint(pipe)) {
				usbDeviceContext->InputPipe = pipe;
			}
			else {
				//
				// The out pipe normal is the first out end point so if we haven't found any then 
				//  it is the first one. 
				// 
				if (usbDeviceContext->OutputPipeNormal == NULL) {
					usbDeviceContext->OutputPipeNormal = pipe;
				}
				else {
					usbDeviceContext->OutputPipeHigh = pipe;
				}
			}

		}
		else {
			LOG_D(" Pipe is 0x%p type %x", pipe, pipeInfo.PipeType);
		}

	}

	WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
	attributes.ParentObject = adapter->WdfDevice;//Mp11GetWdfDevice( adapter );

	ntStatus = WdfWaitLockCreate(&attributes, &usbDeviceContext->PipeStateLock);
	if (!NT_SUCCESS(ntStatus)) {
		LOG_E("Couldn't create PipeStateLock %!STATUS!", ntStatus);
		return ntStatus;

	}
	//ntStatus = AllocateControlRequests(adapter,  MAX_CONTROL_REQUESTS );
	return ntStatus;
}
NDIS_STATUS wf_usb_dev_init(void *adapter)
{
	PADAPTER padapter = adapter;

	wf_usb_dev_create(padapter);

	//wf_usb_xmit_init(padapter);

	//wf_usb_recv_init(padapter);

	return NDIS_STATUS_SUCCESS;
}
#endif

/*----------------------------------------------------------------------------*/
/*!
* \brief This function is a required function that sets up a network adapter,
*        or virtual network adapter, for network I/O operations, claims all
*        hardware resources necessary to the network adapter in the registry,
*        and allocates resources the driver needs to carry out network I/O
         operations.
*
* \param[out] prOpenErrorStatus              Follow MSDN definition.
* \param[out] prSelectedMediumIndex          Follow MSDN definition.
* \param[in]  prMediumArray                  Follow MSDN definition.
* \param[in]  u4MediumArraySize              Follow MSDN definition.
* \param[in]  rMiniportAdapterHandle         Follow MSDN definition.
* \param[in]  rWrapperConfigurationContext   Follow MSDN definition.
*
* \retval NDIS_STATUS_SUCCESS   success
* \retval others                fail for some reasons.
*/
/*----------------------------------------------------------------------------*/
NDIS_STATUS
MPInitialize (
    OUT PNDIS_STATUS prOpenErrorStatus,
    OUT PUINT        prSelectedMediumIndex,
    IN  PNDIS_MEDIUM prMediumArray,
    IN  UINT         u4MediumArraySize,
    IN  NDIS_HANDLE  rMiniportAdapterHandle,
    IN  NDIS_HANDLE  rWrapperConfigurationContext
    )
{
    PADAPTER       prAdapter = NULL;
    P_GLUE_INFO_T     prGlueInfo = NULL;
    UINT        i;
    NDIS_STATUS ndisStatus;
    wf_u8      desc[] = NIC_DRIVER_NAME;
    void       *pvFwImageMapFile = NULL;
    NDIS_HANDLE rFileHandleFwImg = NULL;
    NDIS_STRING rFileWifiRam;
    wf_u32     u4FwImageFileLength = 0;
	wf_mib_info_t* mib_info = wf_malloc(sizeof(wf_mib_info_t));
	P_PARAM_BSSID_EX_T curApInfo = wf_malloc(sizeof(PARAM_BSSID_EX_T));
	

    LOG_D("MPInitialize");
    LOG_D("Current IRQL = %d", KeGetCurrentIrql());
    LOG_D("***** Current Platform: NDIS %d.%d *****", mp_ndis_major, mp_ndis_minor);

    do {
        /* Find the media type we support. */
        for (i = 0; i < u4MediumArraySize; i++) {			
            if (prMediumArray[i] == NdisMedium802_3) {
                break;
            }
        }
        if (i == u4MediumArraySize) {
            LOG_D("Supported media type ==> Not found");
			ndisStatus = NDIS_STATUS_UNSUPPORTED_MEDIA;
            break;
        }

        /* Select ethernet (802.3). */
        *prSelectedMediumIndex = i;

        /* Allocate OS glue object */
        prGlueInfo = windowsCreateGlue(rMiniportAdapterHandle,
            (wf_u16)((wf_u16) mp_ndis_major * 0x100 + (wf_u16) mp_ndis_minor));
        if(prGlueInfo == NULL){
			ndisStatus =  NDIS_STATUS_FAILURE;
            break;
        }

        prGlueInfo->ucDriverDescLen = (wf_u8)strlen(desc) + 1;
        if (prGlueInfo->ucDriverDescLen >= sizeof(prGlueInfo->aucDriverDesc)) {
            prGlueInfo->ucDriverDescLen = sizeof(prGlueInfo->aucDriverDesc);
        }
        strncpy(prGlueInfo->aucDriverDesc, desc, prGlueInfo->ucDriverDescLen);
        prGlueInfo->aucDriverDesc[prGlueInfo->ucDriverDescLen - 1] = '\0';


        prGlueInfo->eParamMediaStateIndicated = PARAM_MEDIA_STATE_DISCONNECTED;
        prGlueInfo->fgIsCardRemoved = FALSE;
		prGlueInfo->bWaitGroupKeyState = FALSE;
        /* Allocate adapter object */
		ndisStatus = MpAllocateAdapter(rMiniportAdapterHandle, &prAdapter);
		
// Set PendedRequest as 1 to always report the status.
		prAdapter->PendedRequest = 1;
		if (ndisStatus != NDIS_STATUS_SUCCESS) {
			break;
		}
		if(mib_info == NULL) {
			LOG_E("malloc mib info failed!");
			break;
		}
		if(curApInfo == NULL) {
			LOG_E("malloc current AP info info failed!");
			break;
		}
		wf_memset(mib_info, 0, sizeof(wf_mib_info_t));
		wf_memset(curApInfo, 0, sizeof(PARAM_BSSID_EX_T));
		mib_info->curApInfo = curApInfo;
		mib_info->writeKeyLock.lock_type = WF_LOCK_TYPE_MUTEX;
		wf_lock_mutex_init(&mib_info->writeKeyLock.lock_mutex);
		prAdapter->mib_info = mib_info;
		prAdapter->dev_state = WF_DEV_STATE_INIT;
        LOG_D("Adapter structure pointer @0x%p", prAdapter);

        /* link glue info and adapter with each other */
		prAdapter->parent = prGlueInfo;
        prGlueInfo->prAdapter = prAdapter;

		ndisStatus = wf_usb_dev_init(prAdapter);
		if(ndisStatus != NDIS_STATUS_SUCCESS) {
            break;
        }

		ndisStatus = wf_nic_dev_init(prAdapter);
		if(ndisStatus != NDIS_STATUS_SUCCESS) {
            break;
        }
		
		ndisStatus = wf_dbg_init(prAdapter);
		if(ndisStatus != NDIS_STATUS_SUCCESS) {
            break;
        }

		ndisStatus = wf_oids_adapt_init(prAdapter);
		if(ndisStatus != NDIS_STATUS_SUCCESS) {
            break;
        }

		ndisStatus = wf_attr_init(prGlueInfo);

		ndisStatus = windowsReadRegistryParameters(prGlueInfo,
                                           rWrapperConfigurationContext);
        LOG_D("windowsReadRegistryParameters() = %08x", ndisStatus);
        if (ndisStatus != NDIS_STATUS_SUCCESS) {
            LOG_E("Read registry parameters FAILED (status=0x%x)", ndisStatus);
            break;
        }
        LOG_D("Read registry parameters -- OK");

        /* Inform NDIS of the attributes of our adapter.
           This has to be done before calling NdisMRegisterXxx or NdisXxxx
           function that depends on the information supplied to
           NdisMSetAttributesEx.
           e.g. NdisMAllocateMapRegisters  */
        NdisMSetAttributesEx(rMiniportAdapterHandle,
	         (NDIS_HANDLE) prGlueInfo,
	         0,
	         (ULONG) (NDIS_ATTRIBUTE_DESERIALIZE | NDIS_ATTRIBUTE_NO_HALT_ON_SUSPEND),
	         NdisInterfaceInternal);
        LOG_D("Set attributes -- OK");
		ndisStatus = wf_usb_dev_start(prAdapter);
        if(ndisStatus != NDIS_STATUS_SUCCESS) {
            break;
        }
		prAdapter->dev_state = WF_DEV_STATE_RUN;
		
		wf_indicate_StatusAndComplete(prGlueInfo,
			NDIS_STATUS_MEDIA_DISCONNECT,
			NULL,
			0);

        /* Register a shutdown handler for NDIS50 or earlier miniports.
           For NDIS 5.1 miniports, set AdapterShutdownHandler as shown
           above. */
        if (prGlueInfo->u2NdisVersion == 0x500)
        {
            NdisMRegisterAdapterShutdownHandler(prGlueInfo->rMiniportAdapterHandle,
                (PVOID) prAdapter, (ADAPTER_SHUTDOWN_HANDLER) mpShutdown);
        }



    } while (FALSE);

    LOG_D("MPInitialize Completed: %08X (%d)", ndisStatus, prGlueInfo->fgIsCardRemoved);

    if (prAdapter && ndisStatus != NDIS_STATUS_SUCCESS) {
        /* Undo everything if it failed. */
        NdisInterlockedDecrement(&prGlueInfo->exitRefCount);
		NdisFreeMemory(prAdapter, sizeof(ADAPTER), 0);
		NdisFreeMemory(prGlueInfo, sizeof(GLUE_INFO_T), 0);
        return ndisStatus;
    }

    return ndisStatus;
}   /* mpInitialize */


NTSTATUS
DriverEntry(PDRIVER_OBJECT DriverObject,PUNICODE_STRING RegistryPath)
{
	NDIS_STATUS                   status;
	NDIS_HANDLE                   ndisWrapperHandle;
	NDIS_MINIPORT_CHARACTERISTICS mpChar;
	NTSTATUS                                ntStatus;
	WDF_DRIVER_CONFIG                       config;
	LOG_D("DriverEntry");
	LOG_D("DriverEntry: Driver object @0x%p\n", DriverObject);
	WDF_DRIVER_CONFIG_INIT(&config, WDF_NO_EVENT_CALLBACK);
	config.DriverInitFlags |= WdfDriverInitNoDispatchOverride;
	LOG_D("IRQL_LEVEL: %d", KeGetCurrentIrql());

	ntStatus = WdfDriverCreate(DriverObject,
		RegistryPath,
		WDF_NO_OBJECT_ATTRIBUTES,
		&config,
		WDF_NO_HANDLE); //vm control
	if (!NT_SUCCESS(ntStatus)) {
		LOG_E("WdfDriverCreate failed");
		return NDIS_STATUS_FAILURE;
	}
	LOG_D("Created WDF driver successfully.");
	/* Now we must initialize the wrapper, and then register the Miniport */
	NdisMInitializeWrapper(&ndisWrapperHandle, DriverObject, RegistryPath, NULL);

	if (ndisWrapperHandle == NULL) {
		status = NDIS_STATUS_FAILURE;
		LOG_E("Init wrapper ==> FAILED (status=0x%x)", status);
		return status;
	}
	NdisZeroMemory(&mpChar, sizeof(NDIS_MINIPORT_CHARACTERISTICS));

	/* Initialize the Miniport characteristics for the call to
	NdisMRegisterMiniport. */
	mpChar.MajorNdisVersion 		= mp_ndis_major;
	mpChar.MinorNdisVersion 		= mp_ndis_minor;
	mpChar.CheckForHangHandler 		= NULL;  //mpCheckForHang;
	mpChar.DisableInterruptHandler 	= NULL;
	mpChar.EnableInterruptHandler 	= NULL;
	mpChar.HaltHandler 				= mpHalt;
	mpChar.HandleInterruptHandler 	= mpHandleInterrupt;
	mpChar.InitializeHandler 		= MPInitialize;
	mpChar.ISRHandler 				= mpIsr;
	mpChar.QueryInformationHandler 	= mpQueryInformation;
	/*mpChar.ReconfigureHandler      = NULL;*/
	mpChar.ResetHandler 			= mpReset;
	mpChar.SetInformationHandler 	= mpSetInformation;
	mpChar.SendHandler 				= NULL;
	mpChar.SendPacketsHandler 		= wf_xmit_ndis_pkt;
	mpChar.ReturnPacketHandler 		= mpReturnPacket;
	mpChar.TransferDataHandler 		= NULL;
	mpChar.AllocateCompleteHandler 	= NULL;
#ifdef NDIS51_MINIPORT
	mpChar.CancelSendPacketsHandler = NULL;
	/*mpChar.CancelSendPacketsHandler = MPCancelSendPackets; */
	mpChar.PnPEventNotifyHandler = NULL;//mpPnPEventNotify;
	mpChar.AdapterShutdownHandler = mpShutdown;
#endif

	/* Register this driver to use the NDIS library of version the same as
	the default setting of the build environment. */
	status = NdisMRegisterMiniport(ndisWrapperHandle, &mpChar, sizeof(NDIS_MINIPORT_CHARACTERISTICS));

	LOG_D("NdisMRegisterMiniport (NDIS %d.%d) returns 0x%x\n", mp_ndis_major, mp_ndis_minor, status);

#ifndef _WIN64
#ifdef NDIS51_MINIPORT
	/* If the current platform cannot support NDIS 5.1, we attempt to declare
	ourselves as an NDIS 5.0 miniport driver. */
	if (status == NDIS_STATUS_BAD_VERSION) {
		mpChar.MinorNdisVersion = 0;
		/* Register this driver to use the NDIS 5.0 library. */
		status = NdisMRegisterMiniport(ndisWrapperHandle, &mpChar,
			sizeof(NDIS50_MINIPORT_CHARACTERISTICS));

		LOG_D("NdisMRegisterMiniport (NDIS %d.%d) returns 0x%x\n", 0, 0, status);
	}
#endif
#endif

	if (status != NDIS_STATUS_SUCCESS) {
		LOG_D("Register NDIS %d.%d miniport ==> FAILED (status=0x%x)\n",
			mp_ndis_major, mp_ndis_minor, status);
		NdisTerminateWrapper(ndisWrapperHandle, NULL);
		return status;
	}
	
	return NDIS_STATUS_SUCCESS;
}

NDIS_STATUS
MpAllocateAdapter(
	__in NDIS_HANDLE MiniportAdapterHandle,
	__deref_out_opt PADAPTER* ppAdapter
)
{
	PADAPTER                pAdapter = NULL;
	//PSTATION                pStation = NULL;
	NTSTATUS                ntStatus;
	NDIS_STATUS             ndisStatus = NDIS_STATUS_SUCCESS;
	ULONG                   nameLength;
	WDF_OBJECT_ATTRIBUTES   attributes;

	*ppAdapter = NULL;

	do
	{
		//
		// Allocate a ADAPTER data structure.
		//

//		MP_ALLOCATE_MEMORY(MiniportAdapterHandle, &pAdapter, sizeof(ADAPTER), MP_MEMORY_TAG);
		pAdapter = wf_malloc(sizeof(ADAPTER));
		if (pAdapter == NULL)
		{
			LOG_E("MpAllocateAdapter: Failed to allocate %d bytes for ADAPTER", sizeof(ADAPTER));
			ndisStatus = NDIS_STATUS_RESOURCES;
			break;
		}
		else {
			NdisZeroMemory(pAdapter, sizeof(ADAPTER));
		}
		pAdapter->nic_info = wf_kzalloc(sizeof(nic_info_st));
		if (pAdapter->nic_info == NULL) {
			LOG_E("malloc nic info failed!\n");
			break;
		}
		else {
			NdisZeroMemory(pAdapter->nic_info, sizeof(nic_info_st));
		}

		pAdapter->MiniportAdapterHandle = MiniportAdapterHandle;
		//pAdapter->dev_state = WF_DEV_STATE_INIT;
		NdisInitializeListHead(&pAdapter->Link);
		NdisInitializeEvent(&pAdapter->MiniportRefCountZeroEvent);  // Will be reset during Restart
		NdisInitializeEvent(&pAdapter->ActiveSendersDoneEvent);
		NdisResetEvent(&pAdapter->ActiveSendersDoneEvent);
		NdisInitializeEvent(&pAdapter->HaltWaitEvent);
		NdisResetEvent(&pAdapter->HaltWaitEvent);
		NdisInitializeEvent(&pAdapter->AllReceivesHaveReturnedEvent);
		NdisResetEvent(&pAdapter->AllReceivesHaveReturnedEvent);
		KeInitializeMutex(&pAdapter->ResetPnPMutex, 0xFFFF);
		//
		// Set power state of miniport to D0.
		//
		//
		// NdisMGetDeviceProperty function enables us to get the:
		// PDO - created by the bus driver to represent our device.
		// FDO - created by NDIS to represent our miniport as a function
		//              driver.
		// NextDeviceObject - deviceobject of another driver (filter)
		//                      attached to us at the bottom.
		//  But if we were to talk to a driver that we
		// are attached to as part of the devicestack then NextDeviceObject
		// would be our target DeviceObject for sending read/write Requests.
		//

		NdisMGetDeviceProperty(MiniportAdapterHandle,
			&pAdapter->Pdo,
			&pAdapter->Fdo,
			&pAdapter->NextDeviceObject,
			NULL,
			NULL);
		ntStatus = IoGetDeviceProperty(pAdapter->Pdo,
			DevicePropertyDeviceDescription,
			NIC_ADAPTER_NAME_SIZE,
			pAdapter->AdapterDesc,
			&nameLength);

		if (!NT_SUCCESS(ntStatus))
		{
			LOG_E("IoGetDeviceProperty failed (0x%x)", ntStatus);
			ndisStatus = NDIS_STATUS_FAILURE;
			break;
		}
		WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, WDF_DEVICE_INFO);

		WdfGetDriver();

		ntStatus = WdfDeviceMiniportCreate(WdfGetDriver(),
			&attributes,
			pAdapter->Fdo,
			pAdapter->NextDeviceObject,
			pAdapter->Pdo,
			&pAdapter->WdfDevice);
		if (!NT_SUCCESS(ntStatus))
		{
			LOG_E("WdfDeviceMiniportCreate failed (0x%x)", ntStatus);
			ndisStatus = NDIS_STATUS_FAILURE;
			break;
		}

		WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
		attributes.ParentObject = pAdapter->WdfDevice;

		ntStatus = WdfSpinLockCreate(&attributes, &pAdapter->SendLock);
		NT_STATUS_TO_NDIS_STATUS(ntStatus, &ndisStatus);
		if (ndisStatus != NDIS_STATUS_SUCCESS) {
			LOG_E("WdfSpinLockCreate   fail!");
			break;
		}

		//
		// Get WDF miniport device context.
		//
		GetWdfDeviceInfo(pAdapter->WdfDevice)->Adapter = pAdapter;

		MpInitializeWorkitem(pAdapter);
		//		ndisStatus = MpAllocateAdapterWorkItem(pAdapter);
		//		if (ndisStatus != NDIS_STATUS_SUCCESS)
		//		{
		//			break;
		//		}
	} while (FALSE);

	if (ndisStatus != NDIS_STATUS_SUCCESS) {
		if (pAdapter) {
			MpFreeAdapter(pAdapter);
			pAdapter = NULL;
		}

		*ppAdapter = NULL;
	}
	else {
		*ppAdapter = pAdapter;
	}

	return ndisStatus;
}

VOID
MpFreeAdapter(
	__in __drv_freesMem(Pool) PADAPTER					 pAdapter
)
{
	//
	// Free the resources allocated for station information
	// 

	if (pAdapter->mib_info != NULL) {
		wf_mib_info_t* mib_info = pAdapter->mib_info;
		if(mib_info->curApInfo != NULL){
			wf_free(mib_info->curApInfo);
			mib_info->curApInfo = NULL;
		}
		wf_free(mib_info);
		pAdapter->mib_info = NULL;
	}
	
	if (pAdapter->nic_info != NULL) {
		wf_free(pAdapter->nic_info);
		pAdapter->nic_info = NULL;
	}

	if (pAdapter->AdapterFwInfo.fw0_usb != NULL) {
		wf_free(pAdapter->AdapterFwInfo.fw0_usb);
		pAdapter->AdapterFwInfo.fw0_usb = NULL;
	}

	if (pAdapter->NdisResetWorkItem)
	{
		WdfObjectDelete(pAdapter->NdisResetWorkItem);
		pAdapter->NdisResetWorkItem = NULL;
	}

	if (pAdapter->Dot11ResetWorkItem)
	{
		WdfObjectDelete(pAdapter->Dot11ResetWorkItem);
		pAdapter->Dot11ResetWorkItem = NULL;
	}

	if(pAdapter->setWpaWorkitem)
    {  
        WdfObjectDelete(pAdapter->setWpaWorkitem);
        pAdapter->setWpaWorkitem = NULL;
    }
	
	if(pAdapter->writeKeyWorkitem)
	{  
		WdfObjectDelete(pAdapter->writeKeyWorkitem);
		pAdapter->writeKeyWorkitem = NULL;
	}

	if (pAdapter->WdfDevice) {
		WdfObjectDelete(pAdapter->WdfDevice);
	}

	wf_free(pAdapter);
}

VOID
MpFreeGlue(P_GLUE_INFO_T prGlueInfo)
{
	//TODO: Finish the de-initialization process.    2021/06/07
	MpFreeAdapter(prGlueInfo->prAdapter);
	prGlueInfo->prAdapter = NULL;

}

NTSTATUS
WfEvtDeviceAdd(
	IN WDFDRIVER        Driver,
	IN PWDFDEVICE_INIT  DeviceInit
)
{

	return STATUS_SUCCESS;
}

NTSTATUS
WfEvtDeviceUnload(
	IN WDFDRIVER        Driver
)
{

	return STATUS_SUCCESS;
}

NDIS_STATUS wf_attr_init(P_GLUE_INFO_T prGlueInfo)
{
	// TODO: Fill the initial values.
	PADAPTER prAdapter = prGlueInfo->prAdapter;
	nic_info_st* pNic_info = (nic_info_st*)prAdapter->nic_info;
	hw_info_st* phw_info = pNic_info->hw_info;
	wf_memcpy(prAdapter->PermanentAddress, phw_info->macAddr, ETH_LENGTH_OF_ADDRESS);
	
	prAdapter->Dot11RunningMode = NET_TYPE_INFRA;

	wf_attr_reset(prAdapter);

	return NDIS_STATUS_SUCCESS;
}

NDIS_STATUS MpInitializeWorkitem(PADAPTER pAdapter)
{
	NDIS_STATUS ndisStatus = NDIS_STATUS_SUCCESS;
	ndisStatus = wf_wpa_workitem_init(pAdapter);
	if(ndisStatus != NDIS_STATUS_SUCCESS) return ndisStatus;
	ndisStatus = wf_writeKey_workitem_init(pAdapter);
	return ndisStatus;
}

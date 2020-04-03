/*********************************************************************
 *
 *                             NOTICE
 *               COPYRIGHT 2019 Revulytics, Inc
 *               ALL RIGHTS RESERVED
 * 
 *      This program is confidential, proprietary, and a trade
 *   secret of Revulytics, Inc. The receipt or possession of
 *     this program does not convey any rights to reproduce or
 *      disclose its contents, or to manufacture, use, or sell
 *       anything that it may describe, in whole or in part, 
 *   without the specific written consent of Revulytics, Inc.
 ********************************************************************/
/**
 * @brief    RUI SDK API macros.
 */

#ifndef RUI_SDK_DEFINES_H
#define RUI_SDK_DEFINES_H

#if _MSC_VER >= 1600
#include <stdint.h>
#else
typedef __int8              int8_t;
typedef __int16             int16_t;
typedef __int32             int32_t;
typedef __int64             int64_t;
typedef unsigned __int8     uint8_t;
typedef unsigned __int16    uint16_t;
typedef unsigned __int32    uint32_t;
typedef unsigned __int64    uint64_t;
#endif

#define RUI_API_VISIBILITY      __declspec(dllimport)
#ifdef RUI_32BIT_BUILD
#define RUI_API_LINKAGE         __cdecl
#define RUI_API_CTOR_LINKAGE
#define RUI_CALLBACK_LINKAGE    __stdcall
#else
#define RUI_API_LINKAGE         __cdecl
#define RUI_API_CTOR_LINKAGE
#define RUI_CALLBACK_LINKAGE    __stdcall
#endif

#define RUI_SDK_STATE_FATAL_ERROR                             -999    ///< Irrecoverable internal fatal error.  No futher API calls should be made.
#define RUI_SDK_STATE_UNINITIALIZED                              0    ///< Instance successfully created (CreateInstance()) but not yet successfully configured (CreateConfig()).
#define RUI_SDK_STATE_CONFIG_INITIALIZED_NOT_STARTED             1    ///< Successfully configured (CreateConfig()) and not yet started (StartSDK()).  Will be normal start.
#define RUI_SDK_STATE_CONFIG_MISSING_OR_CORRUPT_NOT_STARTED      2    ///< Successfully configured (CreateConfig()) and not yet started (StartSDK()).  Will be a New Registration start.
#define RUI_SDK_STATE_STARTED_NEW_REG_RUNNING                    3    ///< Running (StartSDK()) with New Registration in progress, not yet completed.
#define RUI_SDK_STATE_RUNNING                                    4    ///< Running (StartSDK()) with no need for New Registration or with successfully completed New Registration.
#define RUI_SDK_STATE_ABORTED_NEW_REG_PROXY_AUTH_FAILURE         5    ///< Aborted run (StartSDK()) due to failed New Registration (CreateConfig()).
#define RUI_SDK_STATE_ABORTED_NEW_REG_NETWORK_FAILURE            6    ///< Aborted run (StartSDK()) due to failed New Registration (CreateConfig()).
#define RUI_SDK_STATE_ABORTED_NEW_REG_FAILED                     7    ///< Aborted run (StartSDK()) due to failed New Registration (CreateConfig()).
#define RUI_SDK_STATE_SUSPENDED                                  9    ///< Instance has been instructed by RUI Server to back-off.  Will return to Running once back-off cleared.
#define RUI_SDK_STATE_PERMANENTLY_DISABLED                      10    ///< Instance has been instructed by RUI Server to disable.  This is permanent, irrecoverable state.
#define RUI_SDK_STATE_STOPPING_NON_SYNC                         11    ///< Stop in progress (StopSDK()).  Stopping non-Sync-related threads.
#define RUI_SDK_STATE_STOPPING_ALL                              12    ///< Stop in progress (StopSDK()).  Stopping Sync-related threads.
#define RUI_SDK_STATE_STOPPED                                   13    ///< Stop completed (StopSDK()).
#define RUI_SDK_STATE_OPTED_OUT                                 14    ///< Instance has been instructed by client to opt out.

#define RUI_OK                                                   0    ///< Function (which may be synchronous or asynchronous), fully successful during synchronous functionality.

#define RUI_SDK_INTERNAL_ERROR_FATAL                          -999    ///< Error:  Irrecoverable internal fatal error.  No further API calls should be made.
#define RUI_SDK_ABORTED                                       -998    ///< Error:  A required New Registration has failed, and hence the SDK is aborted.  StopSDK and DestroyInstance are possible.

#define RUI_INVALID_SDK_OBJECT                                -100    ///< Parameter validation: SDK Instance parameter is NULL or invalid.
#define RUI_INVALID_PARAMETER_EXPECTED_NON_NULL               -110    ///< Parameter validation: Some API parameter is expected to be non-NULL, and is not.
#define RUI_INVALID_PARAMETER_EXPECTED_NON_EMPTY              -111    ///< Parameter validation: Some API parameter is expected to be non-empty, and is not.
#define RUI_INVALID_PARAMETER_EXPECTED_NO_WHITESPACE          -113    ///< Parameter validation: Some API parameter is expected to be free of white space, and is not.
#define RUI_INVALID_PARAMETER_TOO_LONG                        -114    ///< Parameter validation: Some API parameter violates its allowable maximum length.
#define RUI_INVALID_CONFIG_PATH                               -120    ///< Parameter validation: The configFilePath is not a well-formed directory name.
#define RUI_INVALID_CONFIG_PATH_NONEXISTENT_DIR               -121    ///< Parameter validation: The configFilePath identifies a directory that does not exist.
#define RUI_INVALID_CONFIG_PATH_NOT_WRITABLE                  -122    ///< Parameter validation: The configFilePath identifies a directory that is not writable.
#define RUI_INVALID_PRODUCT_ID                                -130    ///< Parameter validation: The productID is not a well-formed Revulytics Product ID.
#define RUI_INVALID_SERVER_URL                                -140    ///< Parameter validation: The serverURL is not a well-formed URL.
#define RUI_INVALID_PROTOCOL                                  -144    ///< Parameter validation: The protocol is not a legal value.
#define RUI_INVALID_AES_KEY_EXPECTED_EMPTY                    -145    ///< Parameter validation: The AES Key is expected to be NULL/empty, and it is not.
#define RUI_INVALID_AES_KEY_LENGTH                            -146    ///< Parameter validation: The AES Key is not the expected length (32 hex chars).
#define RUI_INVALID_AES_KEY_FORMAT                            -147    ///< Parameter validation: The AES Key is not valid hex encoding.
#define RUI_INVALID_SESSION_ID_EXPECTED_EMPTY                 -150    ///< Parameter validation: The sessionID is expected to be empty, and it was not.
#define RUI_INVALID_SESSION_ID_EXPECTED_NON_EMPTY             -151    ///< Parameter validation: The sessionID is expected to be non-empty, and it was not.
#define RUI_INVALID_SESSION_ID_TOO_SHORT                      -152    ///< Parameter validation: The sessionID violates its allowable minimum length.
#define RUI_INVALID_SESSION_ID_TOO_LONG                       -153    ///< Parameter validation: The sessionID violates its allowable maximum length.
#define RUI_INVALID_SESSION_ID_ALREADY_ACTIVE                 -154    ///< Parameter validation: The sessionID is already currently in use.
#define RUI_INVALID_SESSION_ID_NOT_ACTIVE                     -155    ///< Parameter validation: The sessionID is not currently in use.
#define RUI_INVALID_CUSTOM_PROPERTY_ID                        -160    ///< Parameter validation: The 1-based customPropertyID violates its allowable range.
#define RUI_INVALID_DO_SYNC_VALUE                             -170    ///< Parameter validation: The doSync manual sync flag/limit violates its allowable range.
#define RUI_INVALID_MESSAGE_TYPE                              -180    ///< Parameter validation: The messageType is not an allowable value.
#define RUI_INVALID_LICENSE_DATA_VALUE                        -185    ///< Parameter validation: The license data is not an allowable value.
#define RUI_INVALID_PROXY_CREDENTIALS                         -190    ///< Parameter validation: The proxy username and password are not an allowable combination.
#define RUI_INVALID_PROXY_PORT                                -191    ///< Parameter validation: The proxy port was not valid.

#define RUI_CONFIG_NOT_CREATED                                -200    ///< Function validation: Configuration has not been successfully created.
#define RUI_CONFIG_ALREADY_CREATED                            -201    ///< Function validation: Configuration has already been successfully created.
#define RUI_SDK_NOT_STARTED                                   -210    ///< Function validation: SDK has not been successfully started.
#define RUI_SDK_ALREADY_STARTED                               -211    ///< Function validation: SDK has already been successfully started.
#define RUI_SDK_ALREADY_STOPPED                               -213    ///< Function validation: SDK has already been successfully stopped.

#define RUI_FUNCTION_NOT_AVAIL                                -300    ///< Function validation: Function is not available.
#define RUI_SYNC_ALREADY_RUNNING                              -310    ///< Function validation: A sync with the RUI Server is already running.
#define RUI_TIME_THRESHOLD_NOT_REACHED                        -320    ///< Function validation: The API call frequency threshold (set by the RUI Server) has not been reached.
#define RUI_SDK_SUSPENDED                                     -330    ///< Function validation: The RUI Server has instructed a temporary back-off.
#define RUI_SDK_PERMANENTLY_DISABLED                          -331    ///< Function validation: The RUI Server has instructed a permanent disable.
#define RUI_SDK_OPTED_OUT                                     -332    ///< Function validation: The application has instructed an opt out.

#define RUI_NETWORK_CONNECTION_ERROR                          -400    ///< Network:  Not able to reach the RUI Server.
#define RUI_NETWORK_SERVER_ERROR                              -401    ///< Network:  Error while communicating with the RUI Server.
#define RUI_NETWORK_RESPONSE_INVALID                          -402    ///< Network:  Message format error while communicating with the RUI Server.

#define RUI_TEST_CONNECTION_INVALID_PRODUCT_ID                -420    ///< TestConnection:  Invalid ProductID.
#define RUI_TEST_CONNECTION_MISMATCH                          -421    ///< TestConnection:  Mismatch between URL and ProductID.

#define RUI_PROTOCOL_HTTP_PLUS_ENCRYPTION                        1    ///< Protocol to the RUI Server is HTTP + AES-128 Encrypted payload.
#define RUI_PROTOCOL_HTTPS_WITH_FALLBACK                         2    ///< Protocol to the RUI Server is HTTPS, unless that doesn't work, falling back to HTTP + Encryption.
#define RUI_PROTOCOL_HTTPS                                       3    ///< Protocol to the RUI Server is HTTPS with no fall-back.

#define RUI_KEY_TYPE_UNCHANGED                                  -1    ///< Key Type is Unchanged (when in parameter) or UnKnown (when out parameter).
#define RUI_KEY_TYPE_EVALUATION                                  0    ///< Key Type is Evaluation.
#define RUI_KEY_TYPE_PURCHASED                                   1    ///< Key Type is Purchased.
#define RUI_KEY_TYPE_FREEWARE                                    2    ///< Key Type is Freeware.
#define RUI_KEY_TYPE_UNKNOWN                                     3    ///< Key Type is Unknown.
#define RUI_KEY_TYPE_NFR                                         4    ///< Key Type is Not For Resale.
#define RUI_KEY_TYPE_CUSTOM1                                     5    ///< Key Type is User Defined - Custom1.
#define RUI_KEY_TYPE_CUSTOM2                                     6    ///< Key Type is User Defined - Custom2.
#define RUI_KEY_TYPE_CUSTOM3                                     7    ///< Key Type is User Defined - Custom3.

#define RUI_KEY_STATUS_UNCHANGED                                -1    ///< Key Status is Unchanged (when in parameter) or UnKnown (when out parameter).
#define RUI_KEY_STATUS_NO                                        0    ///< Key Status is No.
#define RUI_KEY_STATUS_YES                                       1    ///< Key Status is Yes.

#define RUI_LICENSE_ARRAY_INDEX_KEY_TYPE                         0    ///< License Array Index for field Key Type. (NOTE:  Order changed from RUI SDK V4).
#define RUI_LICENSE_ARRAY_INDEX_KEY_EXPIRED                      1    ///< License Array Index for field Key Expired Status. (NOTE:  Order changed from RUI SDK V4).
#define RUI_LICENSE_ARRAY_INDEX_KEY_ACTIVE                       2    ///< License Array Index for field Key Activated Status. (NOTE:  Order changed from RUI SDK V4).
#define RUI_LICENSE_ARRAY_INDEX_KEY_BLACKLISTED                  3    ///< License Array Index for field Key Blacklisted Status. (NOTE:  Order changed from RUI SDK V4).
#define RUI_LICENSE_ARRAY_INDEX_KEY_WHITELISTED                  4    ///< License Array Index for field Key White-listed Status. (NOTE:  Order changed from RUI SDK V4).
#define RUI_LICENSE_ARRAY_SIZE                                   5    ///< License Array size.

#define RUI_MESSAGE_TYPE_ANY                                     0    ///< ReachOut™ Message Type any (in parameter only).
#define RUI_MESSAGE_TYPE_TEXT                                    1    ///< ReachOut™ Message Type Text.
#define RUI_MESSAGE_TYPE_URL                                     2    ///< ReachOut™ Message Type URL.

#define RUI_SDK_STOP_SYNC_INDEFINITE_WAIT                        0    ///< Perform Sync during StopSDK with indefinite wait.
#define RUI_SDK_STOP_NO_SYNC                                    -1    ///< Perform no Sync during StopSDK.

#endif

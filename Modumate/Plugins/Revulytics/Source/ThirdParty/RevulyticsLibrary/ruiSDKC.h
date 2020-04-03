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
 * @brief    ADAPTER PATTERN:  Class/functions for C API interface for the RUI SDK.
 * The RUI SDK supports various interfaces, where each interfaces is aligned to the types and conventions of a particular programming
 * language.  A client application uses the RUI SDK by selecting one of the interfaces.  This file contains the API for the C interface,
 * which can be used by either a C or C++ compiler, or other programming language that can interoperate with C.  Each interface is
 * supported by the common definitions in ruiSDKDefines.h.
 */
#ifndef RUI_SDK_C_H
#define RUI_SDK_C_H
#include "ruiSDKDefines.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Opaque type for the RUI SDK instance; returned from ruiCreateInstance() and used as first parameter to other API functions.
 */
struct RUI_API_VISIBILITY RUIInstance;

/**
 * Convenience type for the data associated with event tracking for an exception.
 * @see ruiTrackException()
 */
struct RUI_API_VISIBILITY RUIExceptionEvent
{
    const char* className;
    const char* methodName;
    const char* exceptionMessage;
    const char* stackTrace;
};

/**
 * Convenience type for the data associated with event tracking with custom data.
 * @see ruiTrackEventCustom()
 */
struct RUI_API_VISIBILITY RUINameValuePair
{
    const char* name;
    const char* value;
};

#ifdef __cplusplus
#define RUIINSTANCE       RUIInstance
#define RUIEXCEPTIONEVENT RUIExceptionEvent
#define RUINAMEVALUEPAIR  RUINameValuePair
#else
#define RUIINSTANCE       struct RUIInstance
#define RUIEXCEPTIONEVENT struct RUIExceptionEvent
#define RUINAMEVALUEPAIR  struct RUINameValuePair
#endif

#define RUIRESULT               int32_t
#define RUISTATE                int32_t
#define RUIREACHOUTHANDLER      RUIReachOutHandler
#define RUIREACHOUTCLOSER       RUIReachOutCloser
#define RUIREACHOUTREADYFORNEXT RUIReachOutGetReadyForNext

/**
 * Callback function used to handle a ReachOut™ from the RUI Server when registering a custom ReachOut™ Handler
 * (ruiSetReachOutHandler()).  This callback is called once for each ReachOut™ received.
 *
 * The callback can be implemented synchronously or asynchronously, depending on the handler functionality.  Another ReachOut™
 * will not be delivered to the handler until the handler has returned -AND- the call to RUIReachOutReadyForNext returns non-zero.   
 * Therefore, for a synchronous handler, the RUIReachOutReadyForNext should be ready to return non-zero right after the return from the handler.
 * An asynchronous handler will return non-zero on calls to RUIReachOutReadyForNext whenever it is ready to do so.
 *
 * @see ruiSetReachOutHandler()
 * @see RUIReachOutReadyForNext()
 * @param[in]  arg           The parameter passed to the ruiSetReachOutHandler() function; provides the client a self-managed handle.
 * @param[in]  width         The ReachOut™ data: either a number of pixels or percentage; see ReachOut™ Data for more details.
 * @param[in]  height        The ReachOut™ data: either a number of pixels or percentage; see ReachOut™ Data for more details.
 * @param[in]  position      The ReachOut™ data: 1-9; see ReachOut™ Data for more details.
 * @param[in]  message       The ReachOut™ data: URL or text; see ReachOut™ Data for more details.
 * @returns   Nothing.
 */
typedef RUI_API_VISIBILITY void (RUI_CALLBACK_LINKAGE *RUIReachOutHandler)(void* arg, const char* width, const char* height,
                                                                           int32_t position, const char* message);


/**
* Callback function used to test to see if the ReachOut™ Handler is ready to receive another ReachOut™ message.
* This callback is called periodically by the SDK to test for readiness.
*
* Another ReachOut™ will not be delivered to the handler until this function returns non-zero. This function should continue
* to return non-zero until the next call to RUIReachOutHandler is called.
*
* The RUIReachOutReadyForNext handler should begin with a non-zero return. It should set the RUIReachOutReadyForNext handler to return 0
* when RUIReachOutHandler is called. It should then set the RUIReachOutReadyForNext handler to return non-zero when complete processing
* the RUIReachOutHandler. This may be at the end of the RUIReachOutHandler function (synchronous) or later after the RUIReachOutHandler returns
* (asynchronous).
*
* @see ruiSetReachOutHandler()
* @returns   integer value 0 = not ready for next ReachOut; non-zero indicated ready for next ReachOut.
*/
typedef RUI_API_VISIBILITY int (RUI_CALLBACK_LINKAGE *RUIReachOutGetReadyForNext)(void* arg);

/**
 * Callback function used to handle SDK shutdown when registering a custom ReachOut™ Handler (ruiSetReachOutHandler()).  This
 * callback function will be called exactly once, no matter if callback RUIReachOutHandler was ever called or not.  After
 * RUIReachOutCloser() is called, RUIReachOutHandler() will not be called again.
 *
 * @see ruiSetReachOutHandler()
 * @param[in] arg           The parameter passed to the ruiSetReachOutHandler() function; provides the client a self-managed handle.
 * @returns   Nothing.
 */
typedef RUI_API_VISIBILITY void (RUI_CALLBACK_LINKAGE *RUIReachOutCloser)(void* arg);

/**
 * Creates an instance of the SDK, returning the opaque instance object which is used as the first parameter to other APIs.
 * ruiCreateInstance() must be called first, and must be paired with a call to ruiDestroyInstance() when the client application
 * is done using the RUI SDK.  ruiCreateInstance() does not configure the RUI SDK (ruiCreateConfig()) nor start the RUI SDK
 * (ruiStartSDK()).
 *
 * A typical client will create only a single instance of the RUI SDK.  Creating more than one RUI SDK instance is allowed and
 * is used to support clients that are plug-ins or other scenarios whereby multiple independent clients may co-exist in the same
 * executable.  Multiple RUI SDK instances perform independently of one another with the potential exception of shared or
 * unshared configuration file (ruiCreateConfig()).
 *
 * ruiCreateInstance() is a synchronous function, returning when all functionality is completed.
 *
 * @see ruiCreateConfig()
 * @see ruiStartSDK()
 * @param[in] registerDefaultGraphicalReachOutHandler On platforms that contain a RUI SDK default graphical ReachOut™ handler,
 *            automatically register that handler via ruiSetReachOutHandler().
 * @returns   The opaque handle to the RUI SDK instance, that is used in other API calls.  NULL is returned if there is a
 *            creation error.
 */
RUI_API_VISIBILITY RUIINSTANCE* RUI_API_LINKAGE ruiCreateInstance(bool registerDefaultGraphicalReachOutHandler);
    
/**
 * Destroys an instance of the SDK, created with ruiCreateInstance().  No further calls to the RUI SDK API should be made after
 * calling ruiDestroyInstance().
 *
 * ruiDestroyInstance() is a synchronous function, returning when all functionality is completed.
 *
 * @param[in] ruiInstance The opaque RUI SDK handle returned by ruiCreateInstance().
 * @returns   One of the result codes in ruiSDKDefines.h:
 *   * RUI_OK                                        Function (which may be synchronous or asynchronous), fully successful during synchronous functionality.
 *   * RUI_INVALID_SDK_OBJECT                        Parameter validation: SDK Instance parameter is NULL or invalid.
 *   * RUI_SDK_INTERNAL_ERROR_FATAL                  Error:  Irrecoverable internal fatal error.  No further API calls should be made.
 */
RUI_API_VISIBILITY RUIRESULT RUI_API_LINKAGE ruiDestroyInstance(RUIINSTANCE* ruiInstance);
    
/**
 * Returns the version information for the RUI SDK instance in the supplied string parameter.
 *
 * ruiGetSDKVersion() will always allocate memory (regardless of return code), and the client application is responsible for
 * freeing that memory via ruiFree() when the memory is no longer needed.
 *
 * ruiGetSDKVersion() can be called between ruiCreateInstance() and ruiDestroyInstance(), and can be called more than once.
 *
 * ruiGetSDKVersion() is a synchronous function, returning when all functionality is completed.
 *
 * @see ruiFree()
 * @param[in]  ruiInstance The opaque RUI SDK handle returned by ruiCreateInstance().
 * @param[out] sdkVersion  Receives the version string allocated by the RUI SDK; must be freed via ruiFree().
 * @returns    One of the result codes in ruiSDKDefines.h:
 *   * RUI_OK                                        Function (which may be synchronous or asynchronous), fully successful during synchronous functionality.
 *   * RUI_INVALID_SDK_OBJECT                        Parameter validation: SDK Instance parameter is NULL or invalid.
 *   * RUI_SDK_INTERNAL_ERROR_FATAL                  Error:  Irrecoverable internal fatal error.  No further API calls should be made.
 *   * RUI_INVALID_PARAMETER_EXPECTED_NON_NULL       Parameter validation: Some API parameter is expected to be non-NULL, and is not.
 */
RUI_API_VISIBILITY RUIRESULT RUI_API_LINKAGE ruiGetSDKVersion(RUIINSTANCE* ruiInstance, char** sdkVersion);
    
/**
 * Returns the current state of the RUI SDK instance.  The RUI SDK state can change asynchronously.
 *
 * ruiGetState() can be called between ruiCreateInstance() and ruiDestroyInstance(), and can be called more than once.
 *
 * ruiGetState() is a synchronous function, returning when all functionality is completed.
 *
 * @param[in]  ruiInstance The opaque RUI SDK handle returned by ruiCreateInstance().
 * @returns    One of the state codes in ruiSDKDefines.h:
 *   * RUI_SDK_STATE_FATAL_ERROR                             Irrecoverable internal fatal error.  No further API calls should be made.
 *   * RUI_SDK_STATE_UNINITIALIZED                           Instance successfully created (ruiCreateInstance()) but not yet successfully configured (ruiCreateConfig()).
 *   * RUI_SDK_STATE_CONFIG_INITIALIZED_NOT_STARTED          Successfully configured (ruiCreateConfig()) and not yet started (ruiStartSDK()).  Will be normal start.
 *   * RUI_SDK_STATE_CONFIG_MISSING_OR_CORRUPT_NOT_STARTED   Successfully configured (ruiCreateConfig()) and not yet started (ruiStartSDK()).  Will be a New Registration start.
 *   * RUI_SDK_STATE_STARTED_NEW_REG_RUNNING                 Running (ruiStartSDK()) with New Registration in progress, not yet completed.
 *   * RUI_SDK_STATE_RUNNING                                 Running (ruiStartSDK()) with no need for New Registration or with successfully completed New Registration.
 *   * RUI_SDK_STATE_ABORTED_NEW_REG_PROXY_AUTH_FAILURE      Aborted run (ruiStartSDK()) due to failed New Registration (ruiCreateConfig()).
 *   * RUI_SDK_STATE_ABORTED_NEW_REG_NETWORK_FAILURE         Aborted run (ruiStartSDK()) due to failed New Registration (ruiCreateConfig()).
 *   * RUI_SDK_STATE_ABORTED_NEW_REG_FAILED                  Aborted run (ruiStartSDK()) due to failed New Registration (ruiCreateConfig()).
 *   * RUI_SDK_STATE_SUSPENDED                               Instance has been instructed by RUI Server to back-off.  Will return to Running once back-off cleared.
 *   * RUI_SDK_STATE_PERMANENTLY_DISABLED                    Instance has been instructed by RUI Server to disable.  This is permanent, irrecoverable state.
 *   * RUI_SDK_STATE_STOPPING_NON_SYNC                       Stop in progress (ruiStopSDK()).  Stopping non-Sync-related threads.
 *   * RUI_SDK_STATE_STOPPING_ALL                            Stop in progress (ruiStopSDK()).  Stopping Sync-related threads.
 *   * RUI_SDK_STATE_STOPPED                                 Stop completed (ruiStopSDK()).
 *   * RUI_SDK_STATE_OPTED_OUT                               Instance has been instructed by client to opt out.
 */
RUI_API_VISIBILITY RUISTATE RUI_API_LINKAGE ruiGetState(RUIINSTANCE* ruiInstance);

/**
 * Returns the client ID for the RUI SDK instance in the supplied string parameter.
 *
 * ruiGetClientID() will always allocate memory (regardless of return code), and the client application is responsible for
 * freeing that memory via ruiFree() when the memory is no longer needed.
 *
 * ruiGetClientID() can be called between ruiCreateInstance() and ruiDestroyInstance(), and can be called more than once.
 *
 * ruiGetClientID() is a synchronous function, returning when all functionality is completed.
 *
 * @see ruiFree()
 * @param[in]  ruiInstance The opaque RUI SDK handle returned by ruiCreateInstance().
 * @param[out] clientID  Receives the client ID string allocated by the RUI SDK; must be freed via ruiFree().
 * @returns    One of the result codes in ruiSDKDefines.h:
 *   * RUI_OK                                        Function (which may be synchronous or asynchronous), fully successful during synchronous functionality.
 *   * RUI_INVALID_SDK_OBJECT                        Parameter validation: SDK Instance parameter is NULL or invalid.
 *   * RUI_SDK_INTERNAL_ERROR_FATAL                  Error:  Irrecoverable internal fatal error.  No further API calls should be made.
 *   * RUI_INVALID_PARAMETER_EXPECTED_NON_NULL       Parameter validation: Some API parameter is expected to be non-NULL, and is not.
 */
RUI_API_VISIBILITY RUIRESULT RUI_API_LINKAGE ruiGetClientID(RUIINSTANCE* ruiInstance, char** clientID);

/**
 * Sets a custom ReachOut™ handler.  Any previously registered handler, including the default graphical ReachOut™ handler that
 * may have been registered (ruiCreateInstance()).  If handler is NULL, then all parameters are considered to be NULL.  Setting
 * a handler to NULL effectively removes the current handler, if any, without setting a new handler.
 *
 * ruiSetReachOutHandler() can be called between ruiCreateInstance() and ruiStartSDK(), and can be called more than once.
 *
 * ruiSetReachOutHandler() is a synchronous function, returning when all functionality is completed.
 *
 * @see ruiCreateInstance()
 * @see RUIReachOutHandler()
 * @see RUIReachOutCloser()
 * @param[in]  ruiInstance The opaque RUI SDK handle returned by ruiCreateInstance().
 * @param[in]  handler     The function to be called when a ReachOut™ is received from the RUI Server (RUIReachOutHandler()).
 * @param[in]  closer      The function to be called when the RUI SDK is shutting down (RUIReachOutCloser()).
 * @param[in]  arg         Data to be passed to handler function and closer function; provides the client a self-managed handle.
 * @returns    One of the result codes in ruiSDKDefines.h:
 *   * RUI_OK                                        Function (which may be synchronous or asynchronous), fully successful during synchronous functionality.
 *   * RUI_INVALID_SDK_OBJECT                        Parameter validation: SDK Instance parameter is NULL or invalid.
 *   * RUI_SDK_INTERNAL_ERROR_FATAL                  Error:  Irrecoverable internal fatal error.  No further API calls should be made.
 *   * RUI_SDK_SUSPENDED                             Function validation: The RUI Server has instructed a temporary back-off.
 *   * RUI_SDK_PERMANENTLY_DISABLED                  Function validation: The RUI Server has instructed a permanent disable.
 *   * RUI_SDK_OPTED_OUT                             Function validation: The application has instructed an opt out.
 *   * RUI_SDK_ALREADY_STARTED                       Function validation: SDK has already been successfully started.
 *   * RUI_SDK_ALREADY_STOPPED                       Function validation: SDK has already been successfully stopped.
 */
RUI_API_VISIBILITY RUIRESULT RUI_API_LINKAGE ruiSetReachOutHandler(RUIINSTANCE* ruiInstance, RUIREACHOUTHANDLER handler,
                                                                   RUIREACHOUTCLOSER closer, RUIREACHOUTREADYFORNEXT getReadyForNext, void* arg);

/**
 * Creates a configuration for the RUI SDK instance.  A configuration is passed on a file, specified by configFilePath,
 * productID, and appName.  If two or more RUI SDK instances, in the same process or in different processes, use the same values
 * for these three parameters, then those RUI SDK instances are bound together through a shared configuration.  That is not
 * generally desirable nor recommended.  There is no internal locking across multiple RUI SDK instances to manage that would-be shared
 * configuration file.  Client applications are responsible for synchronizing any file changes that are required.  Please
 * contact Revulytics for more information.
 *
 * The RUI SDK has two communications modes:  HTTPS or HTTP + AES-128 encryption.  The mode is configured by protocol.  When
 * protocol is RUI_PROTOCOL_HTTP_PLUS_ENCRYPTION or RUI_PROTOCOL_HTTPS_WITH_FALLBACK, the AES key must be supplied as a 128-bit
 * hex-encoded string (aesKeyHex).  When protocol is RUI_PROTOCOL_HTTPS, then aesKeyHex must be null/empty.
 *
 * On first execution of a client application, no RUI SDK configuration file will exist.  This situation is detected by the RUI
 * SDK and will result in a New Registration message to the RUI Server at ruiStartSDK().  Once the configuration is received
 * from the RUI Server, the RUI SDK writes the configuration file, that is then used for subsequent client application
 * executions.
 *
 * ruiCreateConfig() must be called after ruiCreateInstance() and before most other APIs and must only be successfully called once.
 *
 * ruiCreateConfig() is a synchronous function, returning when all functionality is completed.
 *
 * @see ruiCreateInstance()
 * @see ruiSetReachOutHandler()
 * @param[in] ruiInstance         The opaque RUI SDK handle returned by ruiCreateInstance().
 * @param[in] configFilePath      The directory to use for the RUI SDK instance's configuration file.  Cannot be null/empty; must
 *                                exist and be writable.
 * @param[in] productID           The Revulytics-supplied product id for this client; 10 digits.
 * @param[in] appName             The customer-supplied application name for this client, to distinguish suites with the same
 *                                productID.  Cannot be null/empty or contain white space; at most 16 UTF-8 characters.
 * @param[in] serverURL           The URL to use for New Registrations.  Subsequent communications from the RUI SDK will either
 *                                use this URL or the URL supplied by the RUI Server).  Cannot be null/empty.  Cannot have a
 *                                protocol prefix (e.g., no "http://" or "https://").
 * @param[in] protocol            Indicates whether HTTP + AES, HTTPS with fall back to HTTP + AES, or HTTPS is used to
 *                                communicate with the RUI Server.
 * @param[in] aesKeyHex           AES Key to use when protocol includes encryption; 32 hex chars (16 bytes, 128 bit) key.
 * @param[in] multiSessionEnabled Indicates whether or not the client will explicitly manage sessionID's via ruiStartSession()
 *                                and ruiStopSession(), and supply those sessionID's to the various event tracking APIs.
 * @param[in] reachOutOnAutoSync  Indicates whether or not a ReachOut™ should be requested as part of each RUI SDK Automatic Sync.
 *                                A ReachOut™ request will be made only if a ReachOut™ handler has been set by registering the
 *                                default graphical handler (ruiCreateInstance()) or a custom handler (ruiSetReachOutHandler()).
 * @returns   One of the result codes in ruiSDKDefines.h:
 *   * RUI_OK                                        Function (which may be synchronous or asynchronous), fully successful during synchronous functionality.
 *   * RUI_INVALID_SDK_OBJECT                        Parameter validation: SDK Instance parameter is NULL or invalid.
 *   * RUI_SDK_INTERNAL_ERROR_FATAL                  Error:  Irrecoverable internal fatal error.  No further API calls should be made.
 *   * RUI_SDK_PERMANENTLY_DISABLED                  Function validation: The RUI Server has instructed a permanent disable.
 *   * RUI_CONFIG_ALREADY_CREATED                    Function validation: Configuration has already been successfully created.
 *   * RUI_INVALID_PARAMETER_EXPECTED_NON_EMPTY      Parameter validation: Some API parameter is expected to be non-empty, and is not.
 *   * RUI_INVALID_PARAMETER_EXPECTED_NO_WHITESPACE  Parameter validation: Some API parameter is expected to be free of white space, and is not.
 *   * RUI_INVALID_PARAMETER_TOO_LONG                Parameter validation: Some API parameter violates its allowable maximum length.
 *   * RUI_INVALID_CONFIG_PATH                       Parameter validation: The configFilePath is not a well-formed directory name.
 *   * RUI_INVALID_CONFIG_PATH_NONEXISTENT_DIR       Parameter validation: The configFilePath identifies a directory that does not exist.
 *   * RUI_INVALID_CONFIG_PATH_NOT_WRITABLE          Parameter validation: The configFilePath identifies a directory that is not writable.
 *   * RUI_INVALID_PRODUCT_ID                        Parameter validation: The productID is not a well-formed Revulytics Product ID.
 *   * RUI_INVALID_SERVER_URL                        Parameter validation: The serverURL is not a well-formed URL.
 *   * RUI_INVALID_PROTOCOL                          Parameter validation: The protocol is not a legal value.
 *   * RUI_INVALID_AES_KEY_EXPECTED_EMPTY            Parameter validation: The AES Key is expected to be NULL/empty, and it not.
 *   * RUI_INVALID_AES_KEY_LENGTH                    Parameter validation: The AES Key is not the expected length (32 hex chars; 16 bytes).
 *   * RUI_INVALID_AES_KEY_FORMAT                    Parameter validation: The AES Key is not valid hex encoding.
 */
RUI_API_VISIBILITY RUIRESULT RUI_API_LINKAGE ruiCreateConfig(RUIINSTANCE* ruiInstance, const char* configFilePath,
                                                             const char* productID, const char* appName, const char* serverURL,
                                                             int32_t protocol, const char* aesKeyHex, bool multiSessionEnabled, bool reachOutOnAutoSync);

/**
* Sets the reachOutOnAutoSync flag.   
*
* ruiSetReachOutOnAutoSync() can be called between ruiCreateConfig() and ruiStopSDK() and can be called zero or more times.
*
* ruiSetReachOutOnAutoSync() is a synchronous function returning when all functionality is completed.
*
* @param[in] ruiInstance                  The opaque RUI SDK handle returned by ruiCreateInstance().
* @param[in] reachOutOnAutoSyncSetting    The boolean value uses to set (true) or clear(false) the reachOutOnAutoSync setting specified in ruiCreateConfig
* @returns   One of the result codes in ruiSDKDefines.h:
*   * RUI_OK                                        Function (which may be synchronous or asynchronous), fully successful during synchronous functionality.
*   * RUI_INVALID_SDK_OBJECT                        Parameter validation: SDK Instance parameter is NULL or invalid.
*   * RUI_SDK_INTERNAL_ERROR_FATAL                  Error:  Irrecoverable internal fatal error.  No further API calls should be made.
*   * RUI_SDK_ABORTED                               Error:  A required New Registration has failed, and hence the SDK is aborted.  StopSDK and DestroySDK are possible.
*   * RUI_SDK_SUSPENDED                             Function validation: The RUI Server has instructed a temporary back-off.
*   * RUI_SDK_PERMANENTLY_DISABLED                  Function validation: The RUI Server has instructed a permanent disable.
*   * RUI_SDK_OPTED_OUT                             Function validation: The application has instructed an opt out.
*   * RUI_CONFIG_NOT_CREATED                        Function validation: Configuration has not been successfully created.
*   * RUI_SDK_ALREADY_STOPPED                       Function validation: SDK has already been successfully stopped.
*/

RUI_API_VISIBILITY RUIRESULT RUI_API_LINKAGE ruiSetReachOutOnAutoSync(RUIINSTANCE *ruiInstance, bool reachOutOnAutoSyncSetting);

/**
 * Sets or clears the data to be used with a proxy.  If there is no proxy between the RUI SDK and the RUI Server,
 * there is no need to use this function.  address can be either null/empty (unnamed proxy) or non-null/empty (named proxy).
 * username and password must both be null/empty (non-authenticating proxy) or both be non-null/empty (authenticating proxy).
 * The port is only used for a named proxy, hence port must be zero if address is null/empty, otherwise port must be non-zero.
 * The RUI SDK uses the proxy data in multiple ways to attempt to communicate via a proxy.  The allowed parameter combinations
 * and their usage are the following:
 *   * address,        port,  username,       password   
 *   * null/empty,     0,     null/empty,     null/empty     - Resets the proxy data to its initial state, no proxy server is
 *                                                             used, and the RUI Server is contacted directly.
 *   * non-null/empty, not 0, null/empty,     null/empty     - Identifies a non-authenticating named proxy that will be used
 *                                                             unless communications fails, then falling back to using no proxy.
 *   * null/empty,     0,     non-null/empty, non-null/empty - Identifies an authenticating unnamed proxy that will be used
 *                                                             unless communications fails, then falling back to using no proxy.
 *   * non-null/empty, not 0, non-null/empty, non-null/empty - Identifies an authenticating named proxy that will be used
 *                                                             unless communications fails, then falling back to using an
 *                                                             authenticating unnamed proxy, then falling back to using no proxy.
 *
 * ruiSetProxy() can be called between ruiCreateConfig() and ruiStopSDK(), and can be called zero or more times.
 *
 * ruiSetProxy() is a synchronous function, returning when all functionality is completed.
 *
 * @param[in] ruiInstance  The opaque RUI SDK handle returned by ruiCreateInstance().
 * @param[in] address      The server name or IP address (dot notation) for the named proxy.
 * @param[in] port         The port for the proxy server; only used with named proxy, port != 0 if and only if address non-null/empty.
 * @param[in] username     The named proxy username; username and password must both be null/empty or both be non-null/empty.
 * @param[in] password     The named proxy password; username and password must both be null/empty or both be non-null/empty.
 * @returns   One of the result codes in ruiSDKDefines.h:
 *   * RUI_OK                                        Function (which may be synchronous or asynchronous), fully successful during synchronous functionality.
 *   * RUI_INVALID_SDK_OBJECT                        Parameter validation: SDK Instance parameter is NULL or invalid.
 *   * RUI_SDK_INTERNAL_ERROR_FATAL                  Error:  Irrecoverable internal fatal error.  No further API calls should be made.
 *   * RUI_SDK_ABORTED                               Error:  A required New Registration has failed, and hence the SDK is aborted.  StopSDK and DestroySDK are possible.
 *   * RUI_SDK_PERMANENTLY_DISABLED                  Function validation: The RUI Server has instructed a permanent disable.
 *   * RUI_SDK_OPTED_OUT                             Function validation: The application has instructed an opt out.
 *   * RUI_INVALID_PROXY_CREDENTIALS                 Parameter validation: The proxy username and password are not an allowable combination.
 *   * RUI_INVALID_PROXY_PORT                        Parameter validation: The proxy port was not valid.
 *   * RUI_CONFIG_NOT_CREATED                        Function validation: Configuration has not been successfully created.
 *   * RUI_SDK_ALREADY_STOPPED                       Function validation: SDK has already been successfully stopped.
 */
RUI_API_VISIBILITY RUIRESULT RUI_API_LINKAGE ruiSetProxy(RUIINSTANCE* ruiInstance, const char* address, uint16_t port, const char* username, const char* password);    

/**
 * Sets or clears the product data.  NOTE:  The product data must be set every time the RUI SDK instance is run.  This is
 * different than V4 of the RUI (Trackerbird) SDK where the supplied product data was stored in the SDK configuration file and if it was
 * not supplied, the values in the configuration file were used.
 *
 * ruiSetProductData() can be called between ruiCreateConfig() and ruiStopSDK() and can be called zero or more times.
 *
 * ruiSetProductData() is a synchronous function returning when all functionality is completed.
 *
 * @see ruiSetProductEdition()
 * @see ruiSetProductLanguage()
 * @see ruiSetProductVersion()
 * @see ruiSetProductBuildNumber()
 * @param[in] ruiInstance         The opaque RUI SDK handle returned by ruiCreateInstance().
 * @param[in] productEdition      The product edition to use in RUI Server reports; null/empty clears the value.
 * @param[in] productLanguage     The product language to use in RUI Server reports; null/empty clears the value.
 * @param[in] productVersion      The product version to use in RUI Server reports; null/empty clears the value.
 * @param[in] productBuildNumber  The product build number to use in RUI Server reports; null/empty clears the value.
 * @returns   One of the result codes in ruiSDKDefines.h:
 *   * RUI_OK                                        Function (which may be synchronous or asynchronous), fully successful during synchronous functionality.
 *   * RUI_INVALID_SDK_OBJECT                        Parameter validation: SDK Instance parameter is NULL or invalid.
 *   * RUI_SDK_INTERNAL_ERROR_FATAL                  Error:  Irrecoverable internal fatal error.  No further API calls should be made.
 *   * RUI_SDK_ABORTED                               Error:  A required New Registration has failed, and hence the SDK is aborted.  StopSDK and DestroySDK are possible.
 *   * RUI_SDK_SUSPENDED                             Function validation: The RUI Server has instructed a temporary back-off.
 *   * RUI_SDK_PERMANENTLY_DISABLED                  Function validation: The RUI Server has instructed a permanent disable.
 *   * RUI_SDK_OPTED_OUT                             Function validation: The application has instructed an opt out.
 *   * RUI_CONFIG_NOT_CREATED                        Function validation: Configuration has not been successfully created.
 *   * RUI_SDK_ALREADY_STOPPED                       Function validation: SDK has already been successfully stopped.
 */
RUI_API_VISIBILITY RUIRESULT RUI_API_LINKAGE ruiSetProductData(RUIINSTANCE* ruiInstance, const char* productEdition, const char* productLanguage,
                                                               const char* productVersion, const char* productBuildNumber);

/**
 * Sets or clears the product data.  NOTE: The product data must be set every time the RUI SDK instance is run.  This is
 * different than V4 of the RUI (Trackerbird) SDK where the supplied product data was stored in the SDK configuration file and if it was
 * not supplied, the values in the configuration file were used.
 *
 * ruiSetProductEdition() can be called between ruiCreateConfig() and ruiStopSDK(), and can be called zero or more times.
 *
 * ruiSetProductEdition() is a synchronous function, returning when all functionality is completed.
 *
 * @see ruiSetProductData()
 * @param[in] ruiInstance         The opaque RUI SDK handle returned by ruiCreateInstance().
 * @param[in] productEdition      The product edition to use in RUI Server reports; null/empty clears the value.
 * @returns   One of the result codes in ruiSDKDefines.h:
 *   * RUI_OK                                        Function (which may be synchronous or asynchronous), fully successful during synchronous functionality.
 *   * RUI_INVALID_SDK_OBJECT                        Parameter validation: SDK Instance parameter is NULL or invalid.
 *   * RUI_SDK_INTERNAL_ERROR_FATAL                  Error:  Irrecoverable internal fatal error.  No further API calls should be made.
 *   * RUI_SDK_ABORTED                               Error:  A required New Registration has failed, and hence the SDK is aborted.  StopSDK and DestroySDK are possible.
 *   * RUI_SDK_SUSPENDED                             Function validation: The RUI Server has instructed a temporary back-off.
 *   * RUI_SDK_PERMANENTLY_DISABLED                  Function validation: The RUI Server has instructed a permanent disable.
 *   * RUI_SDK_OPTED_OUT                             Function validation: The application has instructed an opt out.
 *   * RUI_CONFIG_NOT_CREATED                        Function validation: Configuration has not been successfully created.
 *   * RUI_SDK_ALREADY_STOPPED                       Function validation: SDK has already been successfully stopped.
 */
RUI_API_VISIBILITY RUIRESULT RUI_API_LINKAGE ruiSetProductEdition(RUIINSTANCE* ruiInstance, const char* productEdition);

/**
 * Sets or clears the product data.  NOTE: The product data must be set every time the RUI SDK instance is run.  This is
 * different than V4 of the RUI (Trackerbird) SDK where the supplied product data was stored in the SDK configuration file and if it was
 * not supplied, the values in the configuration file were used.
 *
 * ruiSetProductLanguage() can be called between ruiCreateConfig() and ruiStopSDK(), and can be called zero or more times.
 *
 * ruiSetProductLanguage() is a synchronous function, returning when all functionality is completed.
 *
 * @see ruiSetProductData()
 * @param[in] ruiInstance         The opaque RUI SDK handle returned by ruiCreateInstance().
 * @param[in] productLanguage     The product language to use in RUI Server reports; null/empty clears the value.
 * @returns   One of the result codes in ruiSDKDefines.h:
 *   * RUI_OK                                        Function (which may be synchronous or asynchronous), fully successful during synchronous functionality.
 *   * RUI_INVALID_SDK_OBJECT                        Parameter validation: SDK Instance parameter is NULL or invalid.
 *   * RUI_SDK_INTERNAL_ERROR_FATAL                  Error:  Irrecoverable internal fatal error.  No further API calls should be made.
 *   * RUI_SDK_ABORTED                               Error:  A required New Registration has failed, and hence the SDK is aborted.  StopSDK and DestroySDK are possible.
 *   * RUI_SDK_SUSPENDED                             Function validation: The RUI Server has instructed a temporary back-off.
 *   * RUI_SDK_PERMANENTLY_DISABLED                  Function validation: The RUI Server has instructed a permanent disable.
 *   * RUI_SDK_OPTED_OUT                             Function validation: The application has instructed an opt out.
 *   * RUI_CONFIG_NOT_CREATED                        Function validation: Configuration has not been successfully created.
 *   * RUI_SDK_ALREADY_STOPPED                       Function validation: SDK has already been successfully stopped.
 */
RUI_API_VISIBILITY RUIRESULT RUI_API_LINKAGE ruiSetProductLanguage(RUIINSTANCE* ruiInstance, const char* productLanguage);

/**
 * Sets or clears the product data.  NOTE: The product data must be set every time the RUI SDK instance is run.  This is
 * different than V4 of the RUI (Trackerbird) SDK where the supplied product data was stored in the SDK configuration file and if it was
 * not supplied, the values in the configuration file were used.
 *
 * ruiSetProductVersion() can be called between ruiCreateConfig() and ruiStopSDK(), and can be called zero or more times.
 *
 * ruiSetProductVersion() is a synchronous function, returning when all functionality is completed.
 *
 * @see ruiSetProductData()
 * @param[in] ruiInstance         The opaque RUI SDK handle returned by ruiCreateInstance().
 * @param[in] productVersion      The product version to use in RUI Server reports; null/empty clears the value.
 * @returns   One of the result codes in ruiSDKDefines.h:
 *   * RUI_OK                                        Function (which may be synchronous or asynchronous), fully successful during synchronous functionality.
 *   * RUI_INVALID_SDK_OBJECT                        Parameter validation: SDK Instance parameter is NULL or invalid.
 *   * RUI_SDK_INTERNAL_ERROR_FATAL                  Error:  Irrecoverable internal fatal error.  No further API calls should be made.
 *   * RUI_SDK_ABORTED                               Error:  A required New Registration has failed, and hence the SDK is aborted.  StopSDK and DestroySDK are possible.
 *   * RUI_SDK_SUSPENDED                             Function validation: The RUI Server has instructed a temporary back-off.
 *   * RUI_SDK_PERMANENTLY_DISABLED                  Function validation: The RUI Server has instructed a permanent disable.
 *   * RUI_SDK_OPTED_OUT                             Function validation: The application has instructed an opt out.
 *   * RUI_CONFIG_NOT_CREATED                        Function validation: Configuration has not been successfully created.
 *   * RUI_SDK_ALREADY_STOPPED                       Function validation: SDK has already been successfully stopped.
 */
RUI_API_VISIBILITY RUIRESULT RUI_API_LINKAGE ruiSetProductVersion(RUIINSTANCE* ruiInstance, const char* productVersion);

/**
 * Sets or clears the product data.  NOTE: The product data must be set every time the RUI SDK instance is run.  This is
 * different than V4 of the RUI (Trackerbird) SDK where the supplied product data was stored in the SDK configuration file and if it was
 * not supplied, the values in the configuration file were used.
 *
 * ruiSetProductBuildNumber() can be called between ruiCreateConfig() and ruiStopSDK(), and can be called zero or more times.
 *
 * ruiSetProductBuildNumber() is a synchronous function, returning when all functionality is completed.
 *
 * @see ruiSetProductData()
 * @param[in] ruiInstance         The opaque RUI SDK handle returned by ruiCreateInstance().
 * @param[in] productBuildNumber  The product build number to use in RUI Server reports; null/empty clears the value.
 * @returns   One of the result codes in ruiSDKDefines.h:
 *   * RUI_OK                                        Function (which may be synchronous or asynchronous), fully successful during synchronous functionality.
 *   * RUI_INVALID_SDK_OBJECT                        Parameter validation: SDK Instance parameter is NULL or invalid.
 *   * RUI_SDK_INTERNAL_ERROR_FATAL                  Error:  Irrecoverable internal fatal error.  No further API calls should be made.
 *   * RUI_SDK_ABORTED                               Error:  A required New Registration has failed, and hence the SDK is aborted.  StopSDK and DestroySDK are possible.
 *   * RUI_SDK_SUSPENDED                             Function validation: The RUI Server has instructed a temporary back-off.
 *   * RUI_SDK_PERMANENTLY_DISABLED                  Function validation: The RUI Server has instructed a permanent disable.
 *   * RUI_SDK_OPTED_OUT                             Function validation: The application has instructed an opt out.
 *   * RUI_CONFIG_NOT_CREATED                        Function validation: Configuration has not been successfully created.
 *   * RUI_SDK_ALREADY_STOPPED                       Function validation: SDK has already been successfully stopped.
 */
RUI_API_VISIBILITY RUIRESULT RUI_API_LINKAGE ruiSetProductBuildNumber(RUIINSTANCE* ruiInstance, const char* productBuildNumber);

/**
 * Sets or clears the license data.  The legal parameter values include RUI_KEY_STATUS_UNCHANGED (-1).  NOTE:  Different from
 * the V4 of the RUI SDK, a sessionID parameter can be supplied.
 *
 * ruiSetLicenseData() can be called between ruiCreateConfig() and ruiStopSDK() and can be called zero or more times.  However,
 * the usage requirements of the sessionID parameter are different if ruiSetLicenseData() is called before ruiStartSDK() or
 * called after ruiStartSDK():
 *   * Before ruiStartSDK() regardless of multiSessionEnabled - sessionID must be null/empty.
 *   * After  ruiStartSDK() and multiSessionEnabled = false   - sessionID must be null/empty.  This is similar to event tracking APIs.
 *   * After  ruiStartSDK() and multiSessionEnabled = true    - sessionID must be a current valid value used in ruiStartSession(), or
 *             it can be null/empty.  This is different than normal event tracking APIs, whereby a null/empty value is not allowed.
 *
 * ruiSetLicenseData() can be called while a New Registration is being performed (ruiCreateConfig(), ruiStartSDK()).  However,
 * the event data is not written to the log file until the New Registration completes, and if the New Registration fails, the
 * data will be lost.
 *
 * ruiSetLicenseData() is an asynchronous function, returning immediately with further functionality executed on separate thread(s).
 *
 * @see ruiCheckLicenseKey()
 * @see ruiSetLicenseKey()
 * @param[in] ruiInstance      The opaque RUI SDK handle returned by ruiCreateInstance().
 * @param[in] keyType          The key type for the license.
 * @param[in] keyExpired       The key expired flag for the license.
 * @param[in] keyActivated     The key activated flag for the license.
 * @param[in] keyBlacklisted   The key blacklisted flag for the license.
 * @param[in] keyWhitelisted   The key whitelisted flag for the license.
 * @param[in] sessionID        A session ID complying with above usage (content conditioning and validation rules in ruiStartSession()).
 * @returns   One of the result codes in ruiSDKDefines.h:
 *   * RUI_OK                                        Function (which may be synchronous or asynchronous), fully successful during synchronous functionality.
 *   * RUI_INVALID_SDK_OBJECT                        Parameter validation: SDK Instance parameter is NULL or invalid.
 *   * RUI_SDK_INTERNAL_ERROR_FATAL                  Error:  Irrecoverable internal fatal error.  No further API calls should be made.
 *   * RUI_SDK_ABORTED                               Error:  A required New Registration has failed, and hence the SDK is aborted.  StopSDK and DestroySDK are possible.
 *   * RUI_SDK_SUSPENDED                             Function validation: The RUI Server has instructed a temporary back-off.
 *   * RUI_SDK_PERMANENTLY_DISABLED                  Function validation: The RUI Server has instructed a permanent disable.
 *   * RUI_SDK_OPTED_OUT                             Function validation: The application has instructed an opt out.
 *   * RUI_CONFIG_NOT_CREATED                        Function validation: Configuration has not been successfully created.
 *   * RUI_SDK_ALREADY_STOPPED                       Function validation: SDK has already been successfully stopped.
 *   * RUI_INVALID_SESSION_ID_EXPECTED_EMPTY         Parameter validation: The sessionID is expected to be empty, and it was not.
 *   * RUI_INVALID_SESSION_ID_EXPECTED_NON_EMPTY     Parameter validation: The sessionID is expected to be non-empty, and it was not.
 *   * RUI_INVALID_SESSION_ID_TOO_SHORT              Parameter validation: The sessionID violates its allowable minimum length.
 *   * RUI_INVALID_SESSION_ID_TOO_LONG               Parameter validation: The sessionID violates its allowable maximum length.
 *   * RUI_INVALID_SESSION_ID_NOT_ACTIVE             Parameter validation: The sessionID is not currently in use.
 *   * RUI_INVALID_LICENSE_DATA_VALUE                Parameter validation: The license data is not an allowable value.
 */
RUI_API_VISIBILITY RUIRESULT RUI_API_LINKAGE ruiSetLicenseData(RUIINSTANCE* ruiInstance, int32_t keyType, int32_t keyExpired, int32_t keyActivated,
                                                               int32_t keyBlacklisted, int32_t keyWhitelisted, const char* sessionID);

/**
 * Sets or clears the custom property data.  NOTE: The custom property data must be set every time the RUI SDK instance is run.
 * This is different than V4 of the RUI SDK where the supplied product data was stored in the RUI SDK configuration file and if
 * it was not supplied, the values in the configuration file were used.
 *
 * ruiSetCustomProperty() can be called between ruiCreateConfig() and ruiStopSDK(), and can be called zero or more times.
 *
 * ruiSetCustomProperty() is a synchronous function, returning when all functionality is completed.
 *
 * @see ruiSetProductData()
 * @param[in] ruiInstance      The opaque RUI SDK handle returned by ruiCreateInstance().
 * @param[in] customPropertyID The 1-based index of the custom property to set.
 * @param[in] customValue      The custom property value to use in RUI Server reports; null/empty clears the value.
 * @returns   One of the result codes in ruiSDKDefines.h:
 *   * RUI_OK                                        Function (which may be synchronous or asynchronous), fully successful during synchronous functionality.
 *   * RUI_INVALID_SDK_OBJECT                        Parameter validation: SDK Instance parameter is NULL or invalid.
 *   * RUI_SDK_INTERNAL_ERROR_FATAL                  Error:  Irrecoverable internal fatal error.  No further API calls should be made.
 *   * RUI_SDK_ABORTED                               Error:  A required New Registration has failed, and hence the SDK is aborted.  StopSDK and DestroySDK are possible.
 *   * RUI_SDK_SUSPENDED                             Function validation: The RUI Server has instructed a temporary back-off.
 *   * RUI_SDK_PERMANENTLY_DISABLED                  Function validation: The RUI Server has instructed a permanent disable.
 *   * RUI_SDK_OPTED_OUT                             Function validation: The application has instructed an opt out.
 *   * RUI_CONFIG_NOT_CREATED                        Function validation: Configuration has not been successfully created.
 *   * RUI_SDK_ALREADY_STOPPED                       Function validation: SDK has already been successfully stopped.
 *   * RUI_INVALID_CUSTOM_PROPERTY_ID                Parameter validation: The 1-based customPropertyID violates its allowable range.
 */
RUI_API_VISIBILITY RUIRESULT RUI_API_LINKAGE ruiSetCustomProperty(RUIINSTANCE* ruiInstance, uint32_t customPropertyID, const char* customValue);

/**
 * Starts the RUI SDK.  ruiStartSDK() must be paired with a call to ruiStopSDK().  After the RUI SDK is started, the various
 * event tracking APIs are available to be used.  If ruiCreateConfig() did not detect a configuration file, ruiStartSDK()
 * will perform a New Registration with the RUI Server.  Until a New Registration is complete, the RUI SDK will not be able save
 * event data to a log file or perform synchronization with the RUI Server.  A successful New Registration (or presence of a
 * configuration file) will put the RUI SDK into a normal running state, whereby events are saved to a log file, automatic and
 * manual synchronizations with the RUI Server are possible, and ReachOut™s from the Server are possible.  A failed New
 * Registration will put the RUI SDK into an aborted state, not allowing further activity.
 *
 * ruiStartSDK() must be called after ruiCreateConfig(), and must be called only once.
 *
 * ruiStartSDK() is an asynchronous function, returning immediately with further functionality executed on separate thread(s).
 *
 * @see ruiGetState()
 * @param[in] ruiInstance      The opaque RUI SDK handle returned by ruiCreateInstance().
 * @returns   One of the result codes in ruiSDKDefines.h:
 *   * RUI_OK                                        Function (which may be synchronous or asynchronous), fully successful during synchronous functionality.
 *   * RUI_INVALID_SDK_OBJECT                        Parameter validation: SDK Instance parameter is NULL or invalid.
 *   * RUI_SDK_INTERNAL_ERROR_FATAL                  Error:  Irrecoverable internal fatal error.  No further API calls should be made.
 *   * RUI_SDK_ABORTED                               Error:  A required New Registration has failed, and hence the SDK is aborted.  StopSDK and DestroySDK are possible.
 *   * RUI_SDK_PERMANENTLY_DISABLED                  Function validation: The RUI Server has instructed a permanent disable.
 *   * RUI_CONFIG_NOT_CREATED                        Function validation: Configuration has not been successfully created.
 *   * RUI_SDK_ALREADY_STARTED                       Function validation: SDK has already been successfully started.
 *   * RUI_SDK_ALREADY_STOPPED                       Function validation: SDK has already been successfully stopped.
 */
RUI_API_VISIBILITY RUIRESULT RUI_API_LINKAGE ruiStartSDK(RUIINSTANCE* ruiInstance);

/**
 * Stops the RUI SDK that was started with ruiStartSDK().  If explicit sessions are allowed (multiSessionsEnabled = true in
 * ruiCreateConfig()), then any sessions that have been started with ruiStartSession() that have not been stopped with
 * ruiStopSession() are automatically stopped.  A manual synchronization with the RUI Server, ruiSync(), will be performed at
 * stop depending on the value of doSync:
 *   * -1 : Do not perform a manual synchronization with the RUI Server as part of the stop.
 *   *  0 : Perform a manual synchronization with the RUI Server as part of the stop; wait indefinitely for completion.
 *   * >0 : Perform a manual synchronization with the RUI Server as part of the stop; wait only doSync seconds for completion.
 *
 * ruiStopSDK() must be called after ruiStartSDK() and must be called only once.  After ruiStopSDK() is called, the various
 * event tracking APIs are no longer available.  The only APIs available are ruiGetState() and ruiDestroyInstance().  The RUI
 * SDK cannot be re-started with a subsequent call to ruiStartSDK().
 *
 * ruiStopSDK() is a synchronous function, including the manual synchronization with the RUI Server (if requested), returning
 * when all functionality is completed.
 *
 * @see ruiCreateConfig()
 * @see ruiStartSDK()
 * @see ruiSync()
 * @param[in] ruiInstance  The opaque RUI SDK handle returned by ruiCreateInstance().
 * @param[in] doSync       Indicates whether to do a manual synchronization as part of the stop, and if so, the wait limit.
 * @returns   One of the result codes in ruiSDKDefines.h:
 *   * RUI_OK                                        Function (which may be synchronous or asynchronous), fully successful during synchronous functionality.
 *   * RUI_INVALID_SDK_OBJECT                        Parameter validation: SDK Instance parameter is NULL or invalid.
 *   * RUI_SDK_INTERNAL_ERROR_FATAL                  Error:  Irrecoverable internal fatal error.  No further API calls should be made.
 *   * RUI_SDK_ABORTED                               Error:  A required New Registration has failed, and hence the SDK is aborted.  StopSDK and DestroySDK are possible.
 *   * RUI_CONFIG_NOT_CREATED                        Function validation: Configuration has not been successfully created.
 *   * RUI_SDK_NOT_STARTED                           Function validation: SDK has not been successfully started.
 *   * RUI_SDK_ALREADY_STOPPED                       Function validation: SDK has already been successfully stopped.
 *   * RUI_INVALID_DO_SYNC_VALUE                     Parameter validation: The doSync manual sync flag/limit violates its allowable range.
 */
RUI_API_VISIBILITY RUIRESULT RUI_API_LINKAGE ruiStopSDK(RUIINSTANCE* ruiInstance, int32_t doSync);

/**
 * Performs a manual synchronization with the RUI Server.  The RUI SDK periodically performs automatic synchronizations with
 * the RUI Server.  ruiSync() provides the client an ability to explicitly synchronize with the RUI Server at a specific time.
 * The manual synchronization can request a ReachOut™ with getReachOut.  NOTE:  Similar to reachOutOnAutoSync (ruiCreateConfig()),
 * the ReachOut™ will not be requested if there is no registered handler (ruiCreateInstance() and ruiSetReachOutHandler()).
 *
 * ruiSync() can be called between ruiStartSDK() and ruiStopSDK() and can be called zero or more times.  NOTE: ruiSync() will
 * not be successful if a New Registration is in progress (i.e. ruiCreateConfig() and ruiStartSDK()).  A manual synchronization with
 * the RUI Server can be associated with ruiStopSDK().
 *
 * ruiSync() is an asynchronous function returning immediately with further functionality executed on separate thread(s).
 *
 * @see ruiCreateInstance()
 * @see ruiCreateConfig()
 * @see ruiSetReachOutHandler()
 * @param[in] ruiInstance  The opaque RUI SDK handle returned by ruiCreateInstance().
 * @param[in] getReachOut  Indicates whether a ReachOut™ should be requested from the RUI Server.
 * @returns   One of the result codes in ruiSDKDefines.h:
 *   * RUI_OK                                        Function (which may be synchronous or asynchronous), fully successful during synchronous functionality.
 *   * RUI_INVALID_SDK_OBJECT                        Parameter validation: SDK Instance parameter is NULL or invalid.
 *   * RUI_SDK_INTERNAL_ERROR_FATAL                  Error:  Irrecoverable internal fatal error.  No further API calls should be made.
 *   * RUI_SDK_ABORTED                               Error:  A required New Registration has failed, and hence the SDK is aborted.  StopSDK and DestroySDK are possible.
 *   * RUI_SDK_SUSPENDED                             Function validation: The RUI Server has instructed a temporary back-off.
 *   * RUI_SDK_PERMANENTLY_DISABLED                  Function validation: The RUI Server has instructed a permanent disable.
 *   * RUI_SDK_OPTED_OUT                             Function validation: The application has instructed an opt out.
 *   * RUI_CONFIG_NOT_CREATED                        Function validation: Configuration has not been successfully created.
 *   * RUI_SDK_NOT_STARTED                           Function validation: SDK has not been successfully started.
 *   * RUI_SYNC_ALREADY_RUNNING                      Function validation: A sync with the RUI Server is already running.
 *   * RUI_SDK_ALREADY_STOPPED                       Function validation: SDK has already been successfully stopped.
 *   * RUI_TIME_THRESHOLD_NOT_REACHED                Function validation: The API call frequency threshold (set by the RUI Server) has not been reached.
 */
RUI_API_VISIBILITY RUIRESULT RUI_API_LINKAGE ruiSync(RUIINSTANCE* ruiInstance, bool getReachOut);

/**
 * Starts an explicit session for event tracking in the RUI SDK.  Must be paired with a call to ruiStopSession().  Explicit
 * sessions are allowed only if ruiCreateConfig() was called with multiSessionEnabled = true.  When explicit sessions are
 * enabled, a valid sessionID becomes a required parameter to the event tracking APIs.
 *
 * The content of a sessionID is conditioned and validated (after conditioning) with the following rules:
 *   * Conditioning: All leading white space is removed.
 *   * Conditioning: All trailing white space is removed.
 *   * Conditioning: All internal white space is removed, except spaces (' ') are left as is.
 *   * Validation: Cannot be shorter than 10 UTF-8 characters.
 *   * Validation: Cannot be longer than 64 UTF-8 characters.
 *
 * The resulting conditioned and validated sessionID must be unique (i.e. not already in use).  NOTE: With the above
 * conditioning, two sessionID's that differ only by white space or after the 64th character, will not be unique.  A sessionID
 * should not be re-used for different sessions.
 *
 * ruiStartSession() can be called between ruiStartSDK() and ruiStopSDK(), and can be called zero or more times.
 *
 * ruiStartSession() is a synchronous function, returning when all functionality is completed.
 *
 * @see ruiCreateConfig()
 * @see ruiStopSession()
 * @param[in] ruiInstance  The opaque RUI SDK handle returned by ruiCreateInstance().
 * @param[in] sessionID    The client-created valid and unique session ID to be registered as a new session start.
 * @returns   One of the result codes in ruiSDKDefines.h:
 *   * RUI_OK                                        Function (which may be synchronous or asynchronous), fully successful during synchronous functionality.
 *   * RUI_INVALID_SDK_OBJECT                        Parameter validation: SDK Instance parameter is NULL or invalid.
 *   * RUI_SDK_INTERNAL_ERROR_FATAL                  Error:  Irrecoverable internal fatal error.  No further API calls should be made.
 *   * RUI_SDK_ABORTED                               Error:  A required New Registration has failed, and hence the SDK is aborted.  StopSDK and DestroySDK are possible.
 *   * RUI_SDK_SUSPENDED                             Function validation: The RUI Server has instructed a temporary back-off.
 *   * RUI_SDK_PERMANENTLY_DISABLED                  Function validation: The RUI Server has instructed a permanent disable.
 *   * RUI_SDK_OPTED_OUT                             Function validation: The application has instructed an opt out.
 *   * RUI_CONFIG_NOT_CREATED                        Function validation: Configuration has not been successfully created.
 *   * RUI_SDK_NOT_STARTED                           Function validation: SDK has not been successfully started.
 *   * RUI_FUNCTION_NOT_AVAIL                        Function validation: Function is not available.
 *   * RUI_SDK_ALREADY_STOPPED                       Function validation: SDK has already been successfully stopped.
 *   * RUI_INVALID_SESSION_ID_EXPECTED_NON_EMPTY     Parameter validation: The sessionID is expected to be non-empty, and it was not.
 *   * RUI_INVALID_SESSION_ID_TOO_SHORT              Parameter validation: The sessionID violates its allowable minimum length.
 *   * RUI_INVALID_SESSION_ID_TOO_LONG               Parameter validation: The sessionID violates its allowable maximum length.
 *   * RUI_INVALID_SESSION_ID_ALREADY_ACTIVE         Parameter validation: The sessionID is already currently in use.
 */
RUI_API_VISIBILITY RUIRESULT RUI_API_LINKAGE ruiStartSession(RUIINSTANCE* ruiInstance, const char* sessionID);

/**
 * Stops an explicit session started with ruiStartSession().  Explicit sessions are allowed only if ruiCreateConfig() was called
 * with multiSessionEnabled = true.  Any explicit sessions not ended with a call to ruiStopSession() are automatically ended
 * when ruiStopSDK() is called.
 *
 * ruiStopSession() can be called between ruiStartSDK() and ruiStopSDK(), and can be called zero or more times.
 *
 * ruiStopSession() is a synchronous function, returning when all functionality is completed.
 *
 * @see ruiCreateConfig()
 * @see ruiStartSession()
 * @see ruiStopSDK()
 * @param[in] ruiInstance  The opaque RUI SDK handle returned by ruiCreateInstance().
 * @param[in] sessionID    A current valid session ID to be stopped (content conditioning and validation rules in ruiStartSession()).
 * @returns   One of the result codes in ruiSDKDefines.h:
 *   * RUI_OK                                        Function (which may be synchronous or asynchronous), fully successful during synchronous functionality.
 *   * RUI_INVALID_SDK_OBJECT                        Parameter validation: SDK Instance parameter is NULL or invalid.
 *   * RUI_SDK_INTERNAL_ERROR_FATAL                  Error:  Irrecoverable internal fatal error.  No further API calls should be made.
 *   * RUI_SDK_ABORTED                               Error:  A required New Registration has failed, and hence the SDK is aborted.  StopSDK and DestroySDK are possible.
 *   * RUI_SDK_SUSPENDED                             Function validation: The RUI Server has instructed a temporary back-off.
 *   * RUI_SDK_PERMANENTLY_DISABLED                  Function validation: The RUI Server has instructed a permanent disable.
 *   * RUI_SDK_OPTED_OUT                             Function validation: The application has instructed an opt out.
 *   * RUI_CONFIG_NOT_CREATED                        Function validation: Configuration has not been successfully created.
 *   * RUI_SDK_NOT_STARTED                           Function validation: SDK has not been successfully started.
 *   * RUI_FUNCTION_NOT_AVAIL                        Function validation: Function is not available.
 *   * RUI_SDK_ALREADY_STOPPED                       Function validation: SDK has already been successfully stopped.
 *   * RUI_INVALID_SESSION_ID_EXPECTED_NON_EMPTY     Parameter validation: The sessionID is expected to be non-empty, and it was not.
 *   * RUI_INVALID_SESSION_ID_TOO_SHORT              Parameter validation: The sessionID violates its allowable minimum length.
 *   * RUI_INVALID_SESSION_ID_TOO_LONG               Parameter validation: The sessionID violates its allowable maximum length.
 *   * RUI_INVALID_SESSION_ID_NOT_ACTIVE             Parameter validation: The sessionID is not currently in use.
 */
RUI_API_VISIBILITY RUIRESULT RUI_API_LINKAGE ruiStopSession(RUIINSTANCE* ruiInstance, const char* sessionID);

/**
 * Logs an exception event with the supplied data.  NOTE:  Different than V4 of the RUI (Trackerbird) SDK, ruiTrackException() accepts a
 * sessionID parameter.  The usage requirements of the sessionID parameter are the following:
 *   * multiSessionEnabled = false - sessionID must be null/empty.
 *   * multiSessionEnabled = true  - sessionID must be a current valid value used in ruiStartSession(), or it can be null/empty.
 *             This is different than normal event tracking APIs, whereby a null/empty value is not allowed.
 *
 * The content of exceptionData.className and exceptionData.methodName is conditioned and validated (after conditioning) with
 * the following rules:
 *   * Conditioning: All leading white space is removed.
 *   * Conditioning: All trailing white space is removed.
 *   * Conditioning: All internal white space is removed, except spaces (' ') are left as is.
 *   * Conditioning: Trimmed to at most 128 UTF-8 characters.
 *   * Validation: Cannot be empty.
 *
 * ruiTrackException() can be called between ruiStartSDK() and ruiStopSDK(), and can be called zero or more times.
 *
 * ruiTrackException() can be called while a New Registration is being performed (ruiCreateConfig(), ruiStartSDK()).  However,
 * the event data is not written to the log file until the New Registration completes, and if the New Registration fails, the
 * data will be lost.
 *
 * ruiTrackException() is an asynchronous function, returning immediately with further functionality executed on separate thread(s).
 *
 * @see ruiCreateConfig()
 * @see ruiStartSDK()
 * @see ruiStartSession()
 * @param[in] ruiInstance    The opaque RUI SDK handle returned by ruiCreateInstance().
 * @param[in] exceptionData  The exception data to log:
 *                           * className  - Class catching exception.  Content conditioning and validation rules above.
 *                           * methodName - Method catching exception.  Content conditioning and validation rules above.
 *                           * message    - Exception message.  No content restrictions, but trimmed to a maximum length determined
 *                                          by the RUI Server.
 *                           * stackTrace - Exception stack trace.  No content restrictions, but trimmed to a maximum length
 *                                        - determined by the RUI Server.
 * @param[in] sessionID      A session ID complying with above usage (content conditioning and validation rules in ruiStartSession()).
 * @returns   One of the result codes in ruiSDKDefines.h:
 *   * RUI_OK                                        Function (which may be synchronous or asynchronous), fully successful during synchronous functionality.
 *   * RUI_INVALID_SDK_OBJECT                        Parameter validation: SDK Instance parameter is NULL or invalid.
 *   * RUI_SDK_INTERNAL_ERROR_FATAL                  Error:  Irrecoverable internal fatal error.  No further API calls should be made.
 *   * RUI_SDK_ABORTED                               Error:  A required New Registration has failed, and hence the SDK is aborted.  StopSDK and DestroySDK are possible.
 *   * RUI_SDK_SUSPENDED                             Function validation: The RUI Server has instructed a temporary back-off.
 *   * RUI_SDK_PERMANENTLY_DISABLED                  Function validation: The RUI Server has instructed a permanent disable.
 *   * RUI_SDK_OPTED_OUT                             Function validation: The application has instructed an opt out.
 *   * RUI_CONFIG_NOT_CREATED                        Function validation: Configuration has not been successfully created.
 *   * RUI_SDK_NOT_STARTED                           Function validation: SDK has not been successfully started.
 *   * RUI_SDK_ALREADY_STOPPED                       Function validation: SDK has already been successfully stopped.
 *   * RUI_INVALID_SESSION_ID_EXPECTED_EMPTY         Parameter validation: The sessionID is expected to be empty, and it was not.
 *   * RUI_INVALID_SESSION_ID_TOO_SHORT              Parameter validation: The sessionID violates its allowable minimum length.
 *   * RUI_INVALID_SESSION_ID_TOO_LONG               Parameter validation: The sessionID violates its allowable maximum length.
 *   * RUI_INVALID_SESSION_ID_NOT_ACTIVE             Parameter validation: The sessionID is not currently in use.
 *   * RUI_INVALID_PARAMETER_EXPECTED_NON_EMPTY      Parameter validation: Some API parameter is expected to be non-empty, and is not.
 */
RUI_API_VISIBILITY RUIRESULT RUI_API_LINKAGE ruiTrackException(RUIINSTANCE* ruiInstance, RUIEXCEPTIONEVENT exceptionData, const char* sessionID);

/**
 * Logs a normal event with the supplied data.   The usage requirements of the sessionID parameter are the following:
 *   * multiSessionEnabled = false - sessionID must be null/empty.
 *   * multiSessionEnabled = true  - sessionID must be a current valid value used in ruiStartSession().
 *
 * NOTE:  Unlike V4 of the RUI SDK, there is no concept of extended names (for eventCategory and eventName).  The content of
 * eventCategory and eventName is conditioned and validated (after conditioning) with the following rules:
 *   * Conditioning: All leading white space is removed.
 *   * Conditioning: All trailing white space is removed.
 *   * Conditioning: All internal white space is removed, except spaces (' ') are left as is.
 *   * Conditioning: Trimmed to at most 128 UTF-8 characters.
 *   * Validation: eventCategory can be null/empty; eventName cannot be null/empty.
 *
 * ruiTrackEvent() can be called between ruiStartSDK() and ruiStopSDK(), and can be called zero or more times.
 *
 * ruiTrackEvent() can be called while a New Registration is being performed (ruiCreateConfig(), ruiStartSDK()).  However,
 * the event data is not written to the log file until the New Registration completes, and if the New Registration fails, the
 * data will be lost.
 *
 * ruiTrackEvent() is an asynchronous function, returning immediately with further functionality executed on separate thread(s).
 *
 * @see ruiCreateConfig()
 * @see ruiStartSDK()
 * @see ruiStartSession()
 * @param[in] ruiInstance    The opaque RUI SDK handle returned by ruiCreateInstance().
 * @param[in] eventCategory  Category of the event with content conditioning and validation rules above.
 * @param[in] eventName      Name of the event with content conditioning and validation rules above.
 * @param[in] sessionID      A session ID complying with above usage (content conditioning and validation rules in ruiStartSession()).
 * @returns   One of the result codes in ruiSDKDefines.h:
 *   * RUI_OK                                        Function (which may be synchronous or asynchronous), fully successful during synchronous functionality.
 *   * RUI_INVALID_SDK_OBJECT                        Parameter validation: SDK Instance parameter is NULL or invalid.
 *   * RUI_SDK_INTERNAL_ERROR_FATAL                  Error:  Irrecoverable internal fatal error.  No further API calls should be made.
 *   * RUI_SDK_ABORTED                               Error:  A required New Registration has failed, and hence the SDK is aborted.  StopSDK and DestroySDK are possible.
 *   * RUI_SDK_SUSPENDED                             Function validation: The RUI Server has instructed a temporary back-off.
 *   * RUI_SDK_PERMANENTLY_DISABLED                  Function validation: The RUI Server has instructed a permanent disable.
 *   * RUI_SDK_OPTED_OUT                             Function validation: The application has instructed an opt out.
 *   * RUI_CONFIG_NOT_CREATED                        Function validation: Configuration has not been successfully created.
 *   * RUI_SDK_NOT_STARTED                           Function validation: SDK has not been successfully started.
 *   * RUI_SDK_ALREADY_STOPPED                       Function validation: SDK has already been successfully stopped.
 *   * RUI_INVALID_SESSION_ID_EXPECTED_EMPTY         Parameter validation: The sessionID is expected to be empty, and it was not.
 *   * RUI_INVALID_SESSION_ID_EXPECTED_NON_EMPTY     Parameter validation: The sessionID is expected to be non-empty, and it was not.
 *   * RUI_INVALID_SESSION_ID_TOO_SHORT              Parameter validation: The sessionID violates its allowable minimum length.
 *   * RUI_INVALID_SESSION_ID_TOO_LONG               Parameter validation: The sessionID violates its allowable maximum length.
 *   * RUI_INVALID_SESSION_ID_NOT_ACTIVE             Parameter validation: The sessionID is not currently in use.
 *   * RUI_INVALID_PARAMETER_EXPECTED_NON_EMPTY      Parameter validation: Some API parameter is expected to be non-empty, and is not.
 */
RUI_API_VISIBILITY RUIRESULT RUI_API_LINKAGE ruiTrackEvent(RUIINSTANCE* ruiInstance, const char* eventCategory, const char* eventName,
                                                           const char* sessionID);

/**
 * Logs a normal event with the supplied data, including a custom numeric field.   The usage requirements of the sessionID
 * parameter are the following:
 *   * multiSessionEnabled = false - sessionID must be null/empty.
 *   * multiSessionEnabled = true  - sessionID must be a current valid value used in ruiStartSession().
 *
 * NOTE:  Unlike V4 of the RUI SDK, there is no concept of extended names (for eventCategory and eventName).  The content of
 * eventCategory and eventName is conditioned and validated (after conditioning) with the following rules:
 *   * Conditioning: All leading white space is removed.
 *   * Conditioning: All trailing white space is removed.
 *   * Conditioning: All internal white space is removed, except spaces (' ') are left as is.
 *   * Conditioning: Trimmed to at most 128 UTF-8 characters.
 *   * Validation: eventCategory can be null/empty; eventName cannot be null/empty.
 *
 * ruiTrackEventNumeric() can be called between ruiStartSDK() and ruiStopSDK(), and can be called zero or more times.
 *
 * ruiTrackEventNumeric() can be called while a New Registration is being performed (ruiCreateConfig(), ruiStartSDK()).  However,
 * the event data is not written to the log file until the New Registration completes, and if the New Registration fails, the
 * data will be lost.
 *
 * ruiTrackEventNumeric() is an asynchronous function, returning immediately with further functionality executed on separate thread(s).
 *
 * @see ruiCreateConfig()
 * @see ruiStartSDK()
 * @see ruiStartSession()
 * @param[in] ruiInstance    The opaque RUI SDK handle returned by ruiCreateInstance().
 * @param[in] eventCategory  Category of the event with content conditioning and validation rules above.
 * @param[in] eventName      Name of the event with content conditioning and validation rules above.
 * @param[in] customValue    Custom numeric data associated with the event.
 * @param[in] sessionID      A session ID complying with above usage (content conditioning and validation rules in ruiStartSession()).
 * @returns   One of the result codes in ruiSDKDefines.h:
 *   * RUI_OK                                        Function (which may be synchronous or asynchronous), fully successful during synchronous functionality.
 *   * RUI_INVALID_SDK_OBJECT                        Parameter validation: SDK Instance parameter is NULL or invalid.
 *   * RUI_SDK_INTERNAL_ERROR_FATAL                  Error:  Irrecoverable internal fatal error.  No further API calls should be made.
 *   * RUI_SDK_ABORTED                               Error:  A required New Registration has failed, and hence the SDK is aborted.  StopSDK and DestroySDK are possible.
 *   * RUI_SDK_SUSPENDED                             Function validation: The RUI Server has instructed a temporary back-off.
 *   * RUI_SDK_PERMANENTLY_DISABLED                  Function validation: The RUI Server has instructed a permanent disable.
 *   * RUI_SDK_OPTED_OUT                             Function validation: The application has instructed an opt out.
 *   * RUI_CONFIG_NOT_CREATED                        Function validation: Configuration has not been successfully created.
 *   * RUI_SDK_NOT_STARTED                           Function validation: SDK has not been successfully started.
 *   * RUI_SDK_ALREADY_STOPPED                       Function validation: SDK has already been successfully stopped.
 *   * RUI_INVALID_SESSION_ID_EXPECTED_EMPTY         Parameter validation: The sessionID is expected to be empty, and it was not.
 *   * RUI_INVALID_SESSION_ID_EXPECTED_NON_EMPTY     Parameter validation: The sessionID is expected to be non-empty, and it was not.
 *   * RUI_INVALID_SESSION_ID_TOO_SHORT              Parameter validation: The sessionID violates its allowable minimum length.
 *   * RUI_INVALID_SESSION_ID_TOO_LONG               Parameter validation: The sessionID violates its allowable maximum length.
 *   * RUI_INVALID_SESSION_ID_NOT_ACTIVE             Parameter validation: The sessionID is not currently in use.
 *   * RUI_INVALID_PARAMETER_EXPECTED_NON_EMPTY      Parameter validation: Some API parameter is expected to be non-empty, and is not.
 */
RUI_API_VISIBILITY RUIRESULT RUI_API_LINKAGE ruiTrackEventNumeric(RUIINSTANCE* ruiInstance, const char* eventCategory, const char* eventName,
                                                                  double customValue, const char* sessionID);

/**
 * Logs a normal event with the supplied data, including a custom string field.   The usage requirements of the sessionID
 * parameter are the following:
 *   * multiSessionEnabled = false - sessionID must be null/empty.
 *   * multiSessionEnabled = true  - sessionID must be a current valid value used in ruiStartSession().
 *
 * NOTE:  Unlike V4 of the RUI SDK, there is no concept of extended names (for eventCategory and eventName).  The content of
 * eventCategory and eventName is conditioned and validated (after conditioning) with the following rules:
 *   * Conditioning: All leading white space is removed.
 *   * Conditioning: All trailing white space is removed.
 *   * Conditioning: All internal white space is removed, except spaces (' ') are left as is.
 *   * Conditioning: Trimmed to at most 128 UTF-8 characters.
 *   * Validation: eventCategory can be null/empty; eventName cannot be null/empty.
 *
 * ruiTrackEventText() can be called between ruiStartSDK() and ruiStopSDK(), and can be called zero or more times.
 *
 * ruiTrackEventText() can be called while a New Registration is being performed (i.e. ruiCreateConfig(), ruiStartSDK()).  However,
 * the event data is not written to the log file until the New Registration completes, and if the New Registration fails the
 * data will be lost.
 *
 * ruiTrackEventText() is an asynchronous function, returning immediately with further functionality executed on separate thread(s).
 *
 * @see ruiCreateConfig()
 * @see ruiStartSDK()
 * @see ruiStartSession()
 * @param[in] ruiInstance    The opaque RUI SDK handle returned by ruiCreateInstance().
 * @param[in] eventCategory  Category of the event with content conditioning and validation rules above.
 * @param[in] eventName      Name of the event with content conditioning and validation rules above.
 * @param[in] customValue    Custom text data associated with the event, cannot be null/empty.  Trimmed to a maximum length determined 
                             by the RUI Server.
 * @param[in] sessionID      A session ID complying with above usage (content conditioning and validation rules in ruiStartSession()).
 * @returns   One of the result codes in ruiSDKDefines.h:
 *   * RUI_OK                                        Function (which may be synchronous or asynchronous), fully successful during synchronous functionality.
 *   * RUI_INVALID_SDK_OBJECT                        Parameter validation: SDK Instance parameter is NULL or invalid.
 *   * RUI_SDK_INTERNAL_ERROR_FATAL                  Error:  Irrecoverable internal fatal error.  No further API calls should be made.
 *   * RUI_SDK_ABORTED                               Error:  A required New Registration has failed, and hence the SDK is aborted.  StopSDK and DestroySDK are possible.
 *   * RUI_SDK_SUSPENDED                             Function validation: The RUI Server has instructed a temporary back-off.
 *   * RUI_SDK_PERMANENTLY_DISABLED                  Function validation: The RUI Server has instructed a permanent disable.
 *   * RUI_SDK_OPTED_OUT                             Function validation: The application has instructed an opt out.
 *   * RUI_CONFIG_NOT_CREATED                        Function validation: Configuration has not been successfully created.
 *   * RUI_SDK_NOT_STARTED                           Function validation: SDK has not been successfully started.
 *   * RUI_SDK_ALREADY_STOPPED                       Function validation: SDK has already been successfully stopped.
 *   * RUI_INVALID_SESSION_ID_EXPECTED_EMPTY         Parameter validation: The sessionID is expected to be empty, and it was not.
 *   * RUI_INVALID_SESSION_ID_EXPECTED_NON_EMPTY     Parameter validation: The sessionID is expected to be non-empty, and it was not.
 *   * RUI_INVALID_SESSION_ID_TOO_SHORT              Parameter validation: The sessionID violates its allowable minimum length.
 *   * RUI_INVALID_SESSION_ID_TOO_LONG               Parameter validation: The sessionID violates its allowable maximum length.
 *   * RUI_INVALID_SESSION_ID_NOT_ACTIVE             Parameter validation: The sessionID is not currently in use.
 *   * RUI_INVALID_PARAMETER_EXPECTED_NON_EMPTY      Parameter validation: Some API parameter is expected to be non-empty, and is not.
 */
RUI_API_VISIBILITY RUIRESULT RUI_API_LINKAGE ruiTrackEventText(RUIINSTANCE* ruiInstance, const char* eventCategory, const char* eventName,
                                                               const char* customValue, const char* sessionID);

/**
 * Logs a normal event with the supplied data, including an array of custom name/value pairs.   The usage requirements of the
 * sessionID parameter are the following:
 *   * multiSessionEnabled = false - sessionID must be null/empty.
 *   * multiSessionEnabled = true  - sessionID must be a current valid value used in ruiStartSession().
 *
 * NOTE:  Unlike V4 of the RUI SDK, there is no concept of extended names (for eventCategory and eventName).  The content of
 * eventCategory and eventName is conditioned and validated (after conditioning) with the following rules:
 *   * Conditioning: All leading white space is removed.
 *   * Conditioning: All trailing white space is removed.
 *   * Conditioning: All internal white space is removed, except spaces (' ') are left as is.
 *   * Conditioning: Trimmed to at most 128 UTF-8 characters.
 *   * Validation: eventCategory can be null/empty; eventName cannot be null/empty.
 *
 * ruiTrackEventCustom() can be called between ruiStartSDK() and ruiStopSDK(), and can be called zero or more times.
 *
 * ruiTrackEventCustom() can be called while a New Registration is being performed (ruiCreateConfig(), ruiStartSDK()).  However,
 * the event data is not written to the log file until the New Registration completes, and if the New Registration fails, the
 * data will be lost.
 *
 * ruiTrackEventCustom() is an asynchronous function, returning immediately with further functionality executed on separate thread(s).
 *
 * @see ruiCreateConfig()
 * @see ruiStartSDK()
 * @see ruiStartSession()
 * @param[in] ruiInstance    The opaque RUI SDK handle returned by ruiCreateInstance().
 * @param[in] eventCategory  Category of the event with content conditioning and validation rules above.
 * @param[in] eventName      Name of the event with content conditioning and validation rules above.
 * @param[in] customValue    Custom data associated with the event, cannot be null/empty.  A given name and/or value can be
 *                           null/empty.  A given name cannot contain white space.  All names and values are trimmed to a maximum
 *                           length determined by the RUI Server.
 * @param[in] numValues      The size of the customValue array (i.e., the number of name/value pairs).  Cannot be 0.
 * @param[in] sessionID      A session ID complying with above usage (content conditioning and validation rules in ruiStartSession()).
 * @returns   One of the result codes in ruiSDKDefines.h:
 *   * RUI_OK                                        Function (which may be synchronous or asynchronous), fully successful during synchronous functionality.
 *   * RUI_INVALID_SDK_OBJECT                        Parameter validation: SDK Instance parameter is NULL or invalid.
 *   * RUI_SDK_INTERNAL_ERROR_FATAL                  Error:  Irrecoverable internal fatal error.  No further API calls should be made.
 *   * RUI_SDK_ABORTED                               Error:  A required New Registration has failed, and hence the SDK is aborted.  StopSDK and DestroySDK are possible.
 *   * RUI_SDK_SUSPENDED                             Function validation: The RUI Server has instructed a temporary back-off.
 *   * RUI_SDK_PERMANENTLY_DISABLED                  Function validation: The RUI Server has instructed a permanent disable.
 *   * RUI_SDK_OPTED_OUT                             Function validation: The application has instructed an opt out.
 *   * RUI_CONFIG_NOT_CREATED                        Function validation: Configuration has not been successfully created.
 *   * RUI_SDK_NOT_STARTED                           Function validation: SDK has not been successfully started.
 *   * RUI_SDK_ALREADY_STOPPED                       Function validation: SDK has already been successfully stopped.
 *   * RUI_INVALID_SESSION_ID_EXPECTED_EMPTY         Parameter validation: The sessionID is expected to be empty, and it was not.
 *   * RUI_INVALID_SESSION_ID_EXPECTED_NON_EMPTY     Parameter validation: The sessionID is expected to be non-empty, and it was not.
 *   * RUI_INVALID_SESSION_ID_TOO_SHORT              Parameter validation: The sessionID violates its allowable minimum length.
 *   * RUI_INVALID_SESSION_ID_TOO_LONG               Parameter validation: The sessionID violates its allowable maximum length.
 *   * RUI_INVALID_SESSION_ID_NOT_ACTIVE             Parameter validation: The sessionID is not currently in use.
 *   * RUI_INVALID_PARAMETER_EXPECTED_NON_NULL       Parameter validation: Some API parameter is expected to be non-NULL, and is not.
 *   * RUI_INVALID_PARAMETER_EXPECTED_NON_EMPTY      Parameter validation: Some API parameter is expected to be non-empty, and is not.
 *   * RUI_INVALID_PARAMETER_EXPECTED_NO_WHITESPACE  Parameter validation: Some API parameter is expected to be free of white space, and is not.
 */
RUI_API_VISIBILITY RUIRESULT RUI_API_LINKAGE ruiTrackEventCustom(RUIINSTANCE* ruiInstance, const char* eventCategory, const char* eventName,
                                                                 RUINAMEVALUEPAIR* customValues, uint32_t numValues, const char* sessionID);

/**
 * Tests the connection between the RUI SDK and the RUI Server.  If a valid configuration file exists (ruiCreateConfig()), the
 * URL used for the test will be the one in that file, set by the RUI Server.  Otherwise, the URL used for the test will be one
 * set by the client in the call to ruiCreateConfig().  If set, the proxy is used during the test (ruiSetProxy()).
 *
 * ruiTestConnection() can be called between ruiCreateConfig() and ruiStopSDK(), and can be called zero or more times.
 *
 * ruiTestConnection() is a synchronous function, returning when all functionality is completed.
 *
 * @see ruiCreateConfig()
 * @see ruiSetProxy()
 * @param[in] ruiInstance      The opaque RUI SDK handle returned by ruiCreateInstance().
 * @returns   One of the result codes in ruiSDKDefines.h:
 *   * RUI_OK                                        Function (which may be synchronous or asynchronous), fully successful during synchronous functionality.
 *   * RUI_INVALID_SDK_OBJECT                        Parameter validation: SDK Instance parameter is NULL or invalid.
 *   * RUI_SDK_INTERNAL_ERROR_FATAL                  Error:  Irrecoverable internal fatal error.  No further API calls should be made.
 *   * RUI_SDK_ABORTED                               Error:  A required New Registration has failed, and hence the SDK is aborted.  StopSDK and DestroySDK are possible.
 *   * RUI_SDK_PERMANENTLY_DISABLED                  Function validation: The RUI Server has instructed a permanent disable.
 *   * RUI_SDK_OPTED_OUT                             Function validation: The application has instructed an opt out.
 *   * RUI_CONFIG_NOT_CREATED                        Function validation: Configuration has not been successfully created.
 *   * RUI_SDK_ALREADY_STOPPED                       Function validation: SDK has already been successfully stopped.
 *   * RUI_NETWORK_CONNECTION_ERROR                  Network:  Not able to reach the RUI Server.
 *   * RUI_NETWORK_SERVER_ERROR                      Network:  Error while communicating with the RUI Server.
 *   * RUI_NETWORK_RESPONSE_INVALID                  Network:  Message format error while communicating with the RUI Server.
 *   * RUI_TEST_CONNECTION_INVALID_PRODUCT_ID        TestConnection:  Invalid ProductID.
 *   * RUI_TEST_CONNECTION_MISMATCH                  TestConnection:  Mismatch between URL and ProductID.
 */
RUI_API_VISIBILITY RUIRESULT RUI_API_LINKAGE ruiTestConnection(RUIINSTANCE* ruiInstance);

/**
 * Indicates that the RUI SDK should notify the RUI Server that it is opting out.  No further communications with the RUI Server
 * will be made once the opt out is sent.  The client must still call ruiStartSDK to send the opt out message to the RUI Server.
 *
 * ruiOptOut() can be called between ruiCreateConfig() and ruiStartSDK(), and can be called zero or more times.
 *
 * ruiOptOut() is a synchronous function, returning when all functionality is completed.
 *
 * @see ruiCreateConfig()
 * @see ruiStartSDK()
 * @param[in] ruiInstance      The opaque RUI SDK handle returned by ruiCreateInstance().
 * @returns   One of the result codes in ruiSDKDefines.h:
 *   * RUI_OK                                        Function (which may be synchronous or asynchronous), fully successful during synchronous functionality.
 *   * RUI_INVALID_SDK_OBJECT                        Parameter validation: SDK Instance parameter is NULL or invalid.
 *   * RUI_SDK_INTERNAL_ERROR_FATAL                  Error:  Irrecoverable internal fatal error.  No further API calls should be made.
 *   * RUI_SDK_ABORTED                               Error:  A required New Registration has failed, and hence the SDK is aborted.  StopSDK and DestroySDK are possible.
 *   * RUI_SDK_PERMANENTLY_DISABLED                  Function validation: The RUI Server has instructed a permanent disable.
 *   * RUI_SDK_OPTED_OUT                             Function validation: The application has instructed an opt out.
 *   * RUI_CONFIG_NOT_CREATED                        Function validation: Configuration has not been successfully created.
 *   * RUI_SDK_ALREADY_STARTED                       Function validation: SDK has already been successfully started.
 *   * RUI_SDK_ALREADY_STOPPED                       Function validation: SDK has already been successfully stopped.
 */
RUI_API_VISIBILITY RUIRESULT RUI_API_LINKAGE ruiOptOut(RUIINSTANCE* ruiInstance);

/**
 * Explicitly checks for manual ReachOut™ messages on the RUI Server.   ruiCheckForReachOut()
 * will check for any manual ReachOut™ message type, whereas ruiCheckForReachOutOfType() will check for ReachOut™ messages of a
 * specified type.
 *
 * ruiCheckForReachOut() will always allocate memory (regardless of return code), and the client application is responsible for
 * freeing that memory via ruiFree() when the memory is no longer needed.
 *
 * ruiCheckForReachOut() can be called between ruiStartSDK() and ruiStopSDK(), and can be called zero or more times.
 *
 * ruiCheckForReachOut() is a synchronous function, returning when all functionality is completed.
 *
 * @see ruiCreateInstance()
 * @see ruiSetReachOutHandler()
 * @see ruiCheckForReachOutOfType()
 * @see ruiFree()
 * @param[in]  ruiInstance      The opaque RUI SDK handle returned by ruiCreateInstance().
 * @param[out] message          Receives the ReachOut™ string allocated by the RUI SDK; must be freed via ruiFree().
 * @param[out] messageCount     Receives the message count (including returned message).  No allocation; no ruiFree().
 * @param[out] messageType      Receives the type of returned message.  No allocation; no ruiFree().
 * @returns    One of the result codes in ruiSDKDefines.h:
 *   * RUI_OK                                        Function (which may be synchronous or asynchronous), fully successful during synchronous functionality.
 *   * RUI_INVALID_SDK_OBJECT                        Parameter validation: SDK Instance parameter is NULL or invalid.
 *   * RUI_SDK_INTERNAL_ERROR_FATAL                  Error:  Irrecoverable internal fatal error.  No further API calls should be made.
 *   * RUI_SDK_ABORTED                               Error:  A required New Registration has failed, and hence the SDK is aborted.  StopSDK and DestroySDK are possible.
 *   * RUI_SDK_SUSPENDED                             Function validation: The RUI Server has instructed a temporary back-off.
 *   * RUI_SDK_PERMANENTLY_DISABLED                  Function validation: The RUI Server has instructed a permanent disable.
 *   * RUI_SDK_OPTED_OUT                             Function validation: The application has instructed an opt out.
 *   * RUI_CONFIG_NOT_CREATED                        Function validation: Configuration has not been successfully created.
 *   * RUI_SDK_ALREADY_STOPPED                       Function validation: SDK has already been successfully stopped.
 *   * RUI_INVALID_PARAMETER_EXPECTED_NON_NULL       Parameter validation: Some API parameter is expected to be non-NULL, and is not.
 *   * RUI_TIME_THRESHOLD_NOT_REACHED                Function validation: The API call frequency threshold (set by the RUI Server) has not been reached.
 *   * RUI_NETWORK_CONNECTION_ERROR                  Network:  Not able to reach the RUI Server.
 *   * RUI_NETWORK_SERVER_ERROR                      Network:  Error while communicating with the RUI Server.
 *   * RUI_NETWORK_RESPONSE_INVALID                  Network:  Message format error while communicating with the RUI Server.
 */
RUI_API_VISIBILITY RUIRESULT RUI_API_LINKAGE ruiCheckForReachOut(RUIINSTANCE* ruiInstance, char** message, int32_t* messageCount, int32_t* messageType);

/**
 * Explicitly checks for manual ReachOut™ messages on the RUI Server.  ruiCheckForReachOutOfType()
 * will check for manual ReachOut™ messages of the specified type, whereas ruiCheckForReachOut() will check for manual ReachOut™ messages of
 * any type.
 *
 * ruiCheckForReachOutOfType() will always allocate memory (regardless of return code), and the client application is responsible for
 * freeing that memory via ruiFree() when the memory is no longer needed.
 *
 * ruiCheckForReachOutOfType() can be called between ruiStartSDK() and ruiStopSDK(), and can be called zero or more times.
 *
 * ruiCheckForReachOutOfType() is a synchronous function, returning when all functionality is completed.
 *
 * @see ruiCreateInstance()
 * @see ruiSetReachOutHandler()
 * @see ruiCheckForReachOut()
 * @see ruiFree()
 * @param[in]  ruiInstance      The opaque RUI SDK handle returned by ruiCreateInstance().
 * @param[out] message          Receives the Reachout™ string allocated by the RUI SDK; must be freed via ruiFree().
 * @param[out] messageCount     Receives the message count (including returned message).  No allocation; no ruiFree().
 * @param[in]  messageType      The message type to search for.
 * @returns    One of the result codes in ruiSDKDefines.h:
 *   * RUI_OK                                        Function (which may be synchronous or asynchronous), fully successful during synchronous functionality.
 *   * RUI_INVALID_SDK_OBJECT                        Parameter validation: SDK Instance parameter is NULL or invalid.
 *   * RUI_SDK_INTERNAL_ERROR_FATAL                  Error:  Irrecoverable internal fatal error.  No further API calls should be made.
 *   * RUI_SDK_ABORTED                               Error:  A required New Registration has failed, and hence the SDK is aborted.  StopSDK and DestroySDK are possible.
 *   * RUI_SDK_SUSPENDED                             Function validation: The RUI Server has instructed a temporary back-off.
 *   * RUI_SDK_PERMANENTLY_DISABLED                  Function validation: The RUI Server has instructed a permanent disable.
 *   * RUI_SDK_OPTED_OUT                             Function validation: The application has instructed an opt out.
 *   * RUI_CONFIG_NOT_CREATED                        Function validation: Configuration has not been successfully created.
 *   * RUI_SDK_ALREADY_STOPPED                       Function validation: SDK has already been successfully stopped.
 *   * RUI_INVALID_MESSAGE_TYPE                      Parameter validation: The messageType is not an allowable value.
 *   * RUI_INVALID_PARAMETER_EXPECTED_NON_NULL       Parameter validation: Some API parameter is expected to be non-NULL, and is not.
 *   * RUI_TIME_THRESHOLD_NOT_REACHED                Function validation: The API call frequency threshold (set by the RUI Server) has not been reached.
 *   * RUI_NETWORK_CONNECTION_ERROR                  Network:  Not able to reach the RUI Server.
 *   * RUI_NETWORK_SERVER_ERROR                      Network:  Error while communicating with the RUI Server.
 *   * RUI_NETWORK_RESPONSE_INVALID                  Network:  Message format error while communicating with the RUI Server.
 */
RUI_API_VISIBILITY RUIRESULT RUI_API_LINKAGE ruiCheckForReachOutOfType(RUIINSTANCE* ruiInstance, char** message, int32_t* messageCount, int32_t messageTypeExpected);

/**
 * Checks the RUI Server for the license data for the supplied licenseKey.  Whereas ruiCheckLicenseKey() is a passive check,
 * ruiSetLicenseKey() changes the license key.  The license array has size, indexes and values as specified in ruiSDKDefines.h:
 *   * RUI_LICENSE_ARRAY_INDEX_KEY_TYPE           0
 *   * RUI_LICENSE_ARRAY_INDEX_KEY_EXPIRED        1
 *   * RUI_LICENSE_ARRAY_INDEX_KEY_ACTIVE         2
 *   * RUI_LICENSE_ARRAY_INDEX_KEY_BLACKLISTED    3
 *   * RUI_LICENSE_ARRAY_INDEX_KEY_WHITELISTED    4
 *   * RUI_LICENSE_ARRAY_SIZE                     5
 *
 * ruiCheckLicenseKey() will always allocate memory (regardless of return code), and the client application is responsible for
 * freeing that memory via ruiFree() when the memory is no longer needed.  NOTE:  The order of the license array data has changed
 * from the RUI (Trackerbird) SDK V4.
 *
 * ruiCheckLicenseKey() can be called between ruiStartSDK() and ruiStopSDK(), and can be called zero or more times.
 *
 * ruiCheckLicenseKey() is a synchronous function returning when all functionality is completed.
 *
 * @see ruiSetLicenseData()
 * @see ruiSetLicenseKey()
 * @see ruiFree()
 * @param[in]  ruiInstance      The opaque RUI SDK handle returned by ruiCreateInstance().
 * @param[in]  licenseKey       The license key to check.  Cannot be null/empty.
 * @param[out] licenseArray     Receives the license data allocated by the RUI SDK; must be freed via ruiFree().  NOTE:  Data order changed.
 * @returns    One of the result codes in ruiSDKDefines.h:
 *   * RUI_OK                                        Function (which may be synchronous or asynchronous), fully successful during synchronous functionality.
 *   * RUI_INVALID_SDK_OBJECT                        Parameter validation: SDK Instance parameter is NULL or invalid.
 *   * RUI_SDK_INTERNAL_ERROR_FATAL                  Error:  Irrecoverable internal fatal error.  No further API calls should be made.
 *   * RUI_SDK_ABORTED                               Error:  A required New Registration has failed, and hence the SDK is aborted.  StopSDK and DestroySDK are possible.
 *   * RUI_SDK_SUSPENDED                             Function validation: The RUI Server has instructed a temporary back-off.
 *   * RUI_SDK_PERMANENTLY_DISABLED                  Function validation: The RUI Server has instructed a permanent disable.
 *   * RUI_SDK_OPTED_OUT                             Function validation: The application has instructed an opt out.
 *   * RUI_CONFIG_NOT_CREATED                        Function validation: Configuration has not been successfully created.
 *   * RUI_SDK_ALREADY_STOPPED                       Function validation: SDK has already been successfully stopped.
 *   * RUI_INVALID_PARAMETER_EXPECTED_NON_NULL       Parameter validation: Some API parameter is expected to be non-NULL, and is not.
 *   * RUI_INVALID_PARAMETER_EXPECTED_NON_EMPTY      Parameter validation: Some API parameter is expected to be non-empty, and is not.
 *   * RUI_TIME_THRESHOLD_NOT_REACHED                Function validation: The API call frequency threshold (set by the RUI Server) has not been reached.
 *   * RUI_NETWORK_CONNECTION_ERROR                  Network:  Not able to reach the RUI Server.
 *   * RUI_NETWORK_SERVER_ERROR                      Network:  Error while communicating with the RUI Server.
 *   * RUI_NETWORK_RESPONSE_INVALID                  Network:  Message format error while communicating with the RUI Server.
 */
RUI_API_VISIBILITY RUIRESULT RUI_API_LINKAGE ruiCheckLicenseKey(RUIINSTANCE* ruiInstance, const char* licenseKey, int32_t** licenseArray);

/**
 * Checks the RUI Server for the license data for the supplied licenseKey and sets the current license to licenseKey.
 * Whereas ruiCheckLicenseKey() is a passive check, ruiSetLicenseKey() changes the license key.  The RUI Server always registers
 * the licenseKey even if the RUI Server knows nothing about the licenseKey.  When a new (unknown) licenseKey is registered, the
 * RUI Server sets the license data to keyType RUI_KEY_TYPE_UNKNOWN and the four status flags (blacklisted, whitelisted, expired,
 * activated) to RUI_KEY_STATUS_NO.  The license array has size, indexes and values as specified in ruiSDKDefines.h:
 *   * RUI_LICENSE_ARRAY_INDEX_KEY_TYPE           0
 *   * RUI_LICENSE_ARRAY_INDEX_KEY_EXPIRED        1
 *   * RUI_LICENSE_ARRAY_INDEX_KEY_ACTIVE         2
 *   * RUI_LICENSE_ARRAY_INDEX_KEY_BLACKLISTED    3
 *   * RUI_LICENSE_ARRAY_INDEX_KEY_WHITELISTED    4
 *   * RUI_LICENSE_ARRAY_SIZE                     5
 *
 * NOTE:  Different from the V4 of the RUI SDK, a sessionID parameter can be supplied (ruiCreateConfig()):
 *   * multiSessionEnabled = false   - sessionID must be null/empty.  This is similar to event tracking APIs.
 *   * multiSessionEnabled = true    - sessionID must be a current valid value used in ruiStartSession(), or it can be
 *             null/empty.  This is different than normal event tracking APIs, whereby a null/empty value is not be allowed.
 *
 * ruiSetLicenseKey() will always allocate memory (regardless of return code), and the client application is responsible for
 * freeing that memory via ruiFree() when the memory is no longer needed.  NOTE:  The order of the license array data has changed
 * from the RUI SDK V4.
 *
 * ruiSetLicenseKey() can be called between ruiStartSDK() and ruiStopSDK(), and can be called zero or more times.
 *
 * ruiSetLicenseKey() is primarily a synchronous function, returning once the check with RUI Server has completed.  Some post-
 * processing functionality is performed asynchronously, executed on separate thread(s).
 *
 * @see ruiSetLicenseData()
 * @see ruiCheckLicenseKey()
 * @see ruiFree()
 * @param[in]  ruiInstance      The opaque RUI SDK handle returned by ruiCreateInstance().
 * @param[in]  licenseKey       The license key to check and set as current.  Cannot be null/empty.
 * @param[out] licenseArray     Receives the license data allocated by the RUI SDK; must be freed via ruiFree().  NOTE:  Data order changed.
 * @param[in]  sessionID        A session ID complying with above usage (content conditioning and validation rules in ruiStartSession()).
 * @returns    One of the result codes in ruiSDKDefines.h:
 *   * RUI_OK                                        Function (which may be synchronous or asynchronous), fully successful during synchronous functionality.
 *   * RUI_INVALID_SDK_OBJECT                        Parameter validation: SDK Instance parameter is NULL or invalid.
 *   * RUI_SDK_INTERNAL_ERROR_FATAL                  Error:  Irrecoverable internal fatal error.  No further API calls should be made.
 *   * RUI_SDK_ABORTED                               Error:  A required New Registration has failed, and hence the SDK is aborted.  StopSDK and DestroySDK are possible.
 *   * RUI_SDK_SUSPENDED                             Function validation: The RUI Server has instructed a temporary back-off.
 *   * RUI_SDK_PERMANENTLY_DISABLED                  Function validation: The RUI Server has instructed a permanent disable.
 *   * RUI_SDK_OPTED_OUT                             Function validation: The application has instructed an opt out.
 *   * RUI_CONFIG_NOT_CREATED                        Function validation: Configuration has not been successfully created.
 *   * RUI_SDK_ALREADY_STOPPED                       Function validation: SDK has already been successfully stopped.
 *   * RUI_INVALID_PARAMETER_EXPECTED_NON_NULL       Parameter validation: Some API parameter is expected to be non-NULL, and is not.
 *   * RUI_INVALID_PARAMETER_EXPECTED_NON_EMPTY      Parameter validation: Some API parameter is expected to be non-empty, and is not.
 *   * RUI_INVALID_SESSION_ID_EXPECTED_EMPTY         Parameter validation: The sessionID is expected to be empty, and it was not.
 *   * RUI_INVALID_SESSION_ID_EXPECTED_NON_EMPTY     Parameter validation: The sessionID is expected to be non-empty, and it was not.
 *   * RUI_INVALID_SESSION_ID_TOO_SHORT              Parameter validation: The sessionID violates its allowable minimum length.
 *   * RUI_INVALID_SESSION_ID_TOO_LONG               Parameter validation: The sessionID violates its allowable maximum length.
 *   * RUI_INVALID_SESSION_ID_NOT_ACTIVE             Parameter validation: The sessionID is not currently in use.
 *   * RUI_TIME_THRESHOLD_NOT_REACHED                Function validation: The API call frequency threshold (set by the RUI Server) has not been reached.
 *   * RUI_NETWORK_CONNECTION_ERROR                  Network:  Not able to reach the RUI Server.
 *   * RUI_NETWORK_SERVER_ERROR                      Network:  Error while communicating with the RUI Server.
 *   * RUI_NETWORK_RESPONSE_INVALID                  Network:  Message format error while communicating with the RUI Server.
 */
RUI_API_VISIBILITY RUIRESULT RUI_API_LINKAGE ruiSetLicenseKey(RUIINSTANCE* ruiInstance, const char* licenseKey, int32_t** licenseArray, const char* sessionID);

/**
 * Frees the memory allocated by the RUI SDK.  The APIs that return allocated memory are:  ruiGetSDKVersion(), ruiCheckForReachOut(),
 * ruiCheckForReachOutOfType(), ruiCheckLicenseKey(), and ruiSetLicenseKey().
 *
 * ruiFree() can be called any time and can be called more than once.
 *
 * ruiFree() is a synchronous function, returning when all functionality is completed.
 *
 * @see ruiGetSDKVersion()
 * @see ruiCheckForReachOut()
 * @see ruiCheckForReachOutOfType()
 * @see ruiCheckLicenseKey()
 * @see ruiSetLicenseKey()
 * @param[in] arg      Pointer to allocated memory returned by the RUI SDK in one of the above APIs.
 * @returns   Nothing.
 */
RUI_API_VISIBILITY void RUI_API_LINKAGE ruiFree(void* arg);

#ifdef __cplusplus
} // extern "C"
#endif

#endif

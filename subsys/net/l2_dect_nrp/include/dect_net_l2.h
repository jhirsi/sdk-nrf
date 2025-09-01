/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef ZEPHYR_INCLUDE_NET_NET_DECT_L2_H_
#define ZEPHYR_INCLUDE_NET_NET_DECT_L2_H_

#include <zephyr/net/net_if.h>
#include <zephyr/net/net_l2.h>

#define DECT_NRP_MTU 1280

/**************************************************************************************************/

/**
 * @brief Common cause values
 */
enum dect_status_values {
	/** Success. */
	DECT_MAC_STATUS_OK = 0x0000,
	/** Generic failure. */
	DECT_MAC_STATUS_FAIL = 0x0001,
	/** Invalid parameter. */
	DECT_MAC_STATUS_INVALID_PARAM = 0x0002,
	/** Request not possible in current state. */
	DECT_MAC_STATUS_NOT_ALLOWED = 0x0003,
	/** Operation not possible due to missing configuration. */
	DECT_MAC_STATUS_NO_CONFIG = 0x0004,
	/** Given RD not found, operation cannot be completed. */
	DECT_MAC_STATUS_RD_NOT_FOUND = 0x0005,
	/** Unable to complete request due to temperature too high or low. */
	DECT_MAC_STATUS_TEMP_FAILURE = 0x0006,
	/**
	 * Unable to complete request due to missing radio resources to deliver the message to
	 * target RD.
	 */
	DECT_MAC_STATUS_NO_RESOURCES = 0x0007,
	/** Request failed due to no response from target RD. */
	DECT_MAC_STATUS_NO_RESPONSE = 0x0008,
	/** Request failed because target RD rejected it. */
	DECT_MAC_STATUS_NW_REJECT = 0x0009,
	/** Unable to complete request due to insufficient memory. */
	DECT_MAC_STATUS_NO_MEMORY = 0x000A,
	/** No RSSI scan results for the requested channel exists. */
	DECT_MAC_STATUS_NO_RSSI_RESULTS = 0x000B,

	/** OS error */
	DECT_MAC_STATUS_OS_ERROR = 0xFFFE,

	/** Unknown error */
	DECT_MAC_STATUS_UNKNOWN = 0xFFFF,
};

/**************************************************************************************************/

/* MAC spec: Table 6.4.2.2-1: Network Beacon period */
enum dect_nw_beacon_period {
	DECT_MAC_NW_BEACON_PERIOD_50MS = 0,
	DECT_MAC_NW_BEACON_PERIOD_100MS = 1,
	DECT_MAC_NW_BEACON_PERIOD_500MS = 2,
	DECT_MAC_NW_BEACON_PERIOD_1000MS = 3,
	DECT_MAC_NW_BEACON_PERIOD_1500MS = 4,
	DECT_MAC_NW_BEACON_PERIOD_2000MS = 5,
	DECT_MAC_NW_BEACON_PERIOD_4000MS = 6,
	/* rest are reserved */
};

/* MAC spec: Table 6.4.2.2-1: Cluster Beacon period */
enum dect_cluster_beacon_period {
	DECT_MAC_CLUSTER_BEACON_PERIOD_10MS = 0,
	DECT_MAC_CLUSTER_BEACON_PERIOD_50MS = 1,
	DECT_MAC_CLUSTER_BEACON_PERIOD_100MS = 2,
	DECT_MAC_CLUSTER_BEACON_PERIOD_500MS = 3,
	DECT_MAC_CLUSTER_BEACON_PERIOD_1000MS = 4,
	DECT_MAC_CLUSTER_BEACON_PERIOD_1500MS = 5,
	DECT_MAC_CLUSTER_BEACON_PERIOD_2000MS = 6,
	DECT_MAC_CLUSTER_BEACON_PERIOD_4000MS = 7,
	DECT_MAC_CLUSTER_BEACON_PERIOD_8000MS = 8,
	DECT_MAC_CLUSTER_BEACON_PERIOD_16000MS = 9,
	DECT_MAC_CLUSTER_BEACON_PERIOD_32000MS = 10,
	/* rest are reserved */
};

/**************************************************************************************************/

#define DECT_MAC_MAX_CHANNELS_IN_RSSI_SCAN	   20
#define DECT_MAC_MAX_CHANNELS_IN_NETWORK_SCAN_REQ  4
#define DECT_MAC_MAX_ADDITIONAL_NW_BEACON_CHANNELS 3

struct dect_rssi_scan_params {
	uint8_t band;
	uint8_t frame_count_to_scan;
	uint8_t channel_count;
	uint16_t channel_list[DECT_MAC_MAX_CHANNELS_IN_RSSI_SCAN];
};
struct dect_scan_params {
	uint8_t band;
	uint8_t channel_count;
	uint16_t channel_list[DECT_MAC_MAX_CHANNELS_IN_NETWORK_SCAN_REQ];
	uint16_t channel_scan_time_ms;
};

enum dect_scan_result_evt_type {
	DECT_SCAN_RESULT_TYPE_CLUSTER_BEACON = 0,
	DECT_SCAN_RESULT_TYPE_NW_BEACON,
};

struct dect_network_beacon_data {
	/** Current cluster channel */
	uint16_t current_cluster_channel;

	/** Next cluster channel */
	uint16_t next_cluster_channel;

	/** Number of additional network beacon channels included in beacon */
	uint8_t num_network_beacon_channels;

	/** Network beacon channels */
	uint16_t network_beacon_channels[DECT_MAC_MAX_ADDITIONAL_NW_BEACON_CHANNELS];
};

/** Received signal information */
struct dect_rx_signal_info {
	/** MCS index. See Table A-1 in @ref DECT-SPEC "DECT-2020 NR Part 4". */
	uint8_t mcs;
	/**
	 * Transmit power, [0,15].
	 * See table 6.2.1-3a in @ref DECT-SPEC "DECT-2020 NR Part 2".
	 */
	uint8_t transmit_power;
	/**
	 * Received signal strength indicator (RSSI-2) of PDC, value in dBm.
	 */
	int8_t rssi_2;
	/**
	 * Received signal to noise indicator (SNR) of PDC, value in dB.
	 */
	int8_t snr;
};

/** @brief dect scan result event, each result is provided to the net_mgmt_event_callback
 * via its info attribute (see net_mgmt.h)
 */
struct dect_scan_result_evt {
	enum dect_scan_result_evt_type beacon_type;

	/** Reception channel number */
	uint16_t channel;
	/** Transmitter Short Radio Device ID */
	uint16_t transmitter_short_rd_id;
	/** Transmitter Long Radio Device ID */
	uint32_t transmitter_long_rd_id;
	/** Network ID */
	uint32_t network_id;

	/** Received signal information */
	struct dect_rx_signal_info rx_signal_info;

	struct dect_network_beacon_data network_beacon;
};

/** Scan result callback
 *
 * @param iface Network interface
 * @param status Scan result status
 * @param entry Scan result entry
 */
typedef void (*dect_scan_result_cb_t)(struct net_if *iface, enum dect_status_values status,
				      struct dect_scan_result_evt *entry);

/**************************************************************************************************/

struct dect_associate_req_params {
	uint32_t target_long_rd_id;
};

struct dect_associate_rel_params {
	uint32_t target_long_rd_id;
};

/**************************************************************************************************/

/** Any suitable channel on set band */
#define DECT_CLUSTER_CHANNEL_ANY 0
struct dect_cluster_start_req_params {
	uint16_t channel;
};

struct dect_cluster_reconfig_req_params {
	uint16_t channel;
	int8_t max_beacon_tx_power_dbm;
	int8_t max_cluster_power_dbm;
	enum dect_cluster_beacon_period period;
};

struct dect_cluster_stop_req_params {
};

struct dect_neighbor_info_req_params {
	uint32_t long_rd_id;
};

/**************************************************************************************************/

struct dect_nw_beacon_start_req_params {
	uint16_t channel;
	uint8_t additional_ch_count;
	uint16_t additional_ch_list[DECT_MAC_MAX_ADDITIONAL_NW_BEACON_CHANNELS];
};

struct dect_nw_beacon_stop_req_params {
};

/**************************************************************************************************/

/**
 * @brief Association reject causes. MAC spec ch 6.4.2.6
 */
enum dect_association_reject_cause {
	/** No radio capacity */
	DECT_MAC_ASSOCIATION_REJECT_CAUSE_NO_RADIO_CAPACITY = 0,
	/** No hardware capacity */
	DECT_MAC_ASSOCIATION_REJECT_CAUSE_NO_HW_CAPACITY = 1,
	/** Conflicted short ID */
	DECT_MAC_ASSOCIATION_REJECT_CAUSE_CONFLICTED_SHORT_ID = 2,
	/** Security needed */
	DECT_MAC_ASSOCIATION_REJECT_CAUSE_SECURITY_NEEDED = 3,
	/** Other reason */
	DECT_MAC_ASSOCIATION_REJECT_CAUSE_OTHER_REASON = 5,
	/** Other reasons than from mac spec  */
	DECT_MAC_ASSOCIATION_NO_RESPONSE = 6,
};

enum dect_rssi_scan_result_verdict {
	DECT_RSSI_SCAN_VERDICT_UNKNOWN,
	DECT_RSSI_SCAN_VERDICT_FREE,
	DECT_RSSI_SCAN_VERDICT_POSSIBLE,
	DECT_RSSI_SCAN_VERDICT_BUSY,
};

/** @brief dect rssi scan result event
 */
struct dect_rssi_scan_result_data {
	uint16_t channel;
	enum dect_rssi_scan_result_verdict frame_subslot_verdicts[48];
	bool all_subslots_free;
	bool another_cluster_detected_in_channel;
	uint8_t scan_suitable_percent;
	uint8_t free_subslot_cnt;
	uint8_t possible_subslot_cnt;
	uint8_t busy_subslot_cnt;

	/** Percentage of busy subslots over measured frames [0 .. 100] */
	uint8_t busy_percentage;

};

struct dect_rssi_scan_result_evt {
	struct dect_rssi_scan_result_data rssi_scan_result;
};

/** @brief dect association resp event
 */
struct dect_association_req_result_evt {
	/** Transmitter Long Radio Device ID */
	uint32_t transmitter_long_rd_id;

	/** Association status */
	bool accepted;

	/** Association reject cause if not accepted */
	enum dect_association_reject_cause reject_cause;
};

/**
 * @brief Association release causes (MAC spec table 6.4.2.6-1).
 */
enum dect_association_release_cause {
	/** Connection termination */
	DECT_MAC_RELEASE_CAUSE_CONNECTION_TERMINATION = 0,
	/** Mobility */
	DECT_MAC_RELEASE_CAUSE_MOBILITY,
	/** Radio device has been inactive for too long */
	DECT_MAC_RELEASE_CAUSE_LONG_INACTIVITY,
	/** Incompatible configuration */
	DECT_MAC_RELEASE_CAUSE_INCOMPATIBLE_CONFIGURATION,
	/** Insufficient hardware resources */
	DECT_MAC_RELEASE_CAUSE_INSUFFICIENT_HW_RESOURCES,
	/** Insufficient radio resources */
	DECT_MAC_RELEASE_CAUSE_INSUFFICIENT_RADIO_RESOURCES,
	/** Bad radio quality */
	DECT_MAC_RELEASE_CAUSE_BAD_RADIO_QUALITY,
	/** Security error */
	DECT_MAC_RELEASE_CAUSE_SECURITY_ERROR,
	/** Other error */
	DECT_MAC_RELEASE_CAUSE_OTHER_ERROR,
	/** Other reason */
	DECT_MAC_RELEASE_CAUSE_OTHER_REASON,
	/** RACH resource failure (additional cause, not from mac spec) */
	DECT_MAC_RELEASE_CAUSE_CUSTOM_RACH_RESOURCE_FAILURE,
};

/** @brief dect association released event
 */
struct dect_association_released_evt {
	/** Long Radio Device ID of neighbor */
	uint32_t long_rd_id;

	/** Cause of the association release */
	enum dect_association_release_cause release_cause;
};
/** @brief dect cluster start resp event
 */
struct dect_cluster_start_resp_evt {
	enum dect_status_values status;
	uint16_t cluster_channel;
};

/** @brief dect network status event
 */
struct dect_network_status_evt {
	enum dect_network_status {
		/** Request failed */
		DECT_NETWORK_STATUS_FAILURE,
		/** Network created */
		DECT_NETWORK_STATUS_CREATED,
		/** Network removed */
		DECT_NETWORK_STATUS_REMOVED,
		/** Network joined */
		DECT_NETWORK_STATUS_JOINED,
		/** Network unjoined */
		DECT_NETWORK_STATUS_UNJOINED,
	} network_status;
	enum dect_status_values dect_err_cause; /* DECT error cause, if network_status is FAILURE */
	int os_err_cause; /* OS error cause, if dect_err_cause is DECT_MAC_STATUS_OS_ERROR */
};

/** @brief dect sink status event
 */
struct dect_sink_status_evt {
	enum dect_sink_status {
		/** Sink connected */
		DECT_SINK_STATUS_CONNECTED,
		/** Sink disconnected */
		DECT_SINK_STATUS_DISCONNECTED,
	} sink_status;
	/** Interface towards Border Router / Internet */
	struct net_if *br_iface;
};

/* Note: change also CONFIG_NET_MGMT_EVENT_INFO_DEFAULT_DATA_SIZE if changing dect_neighbor_list_evt
 */
#define DECT_L2_MAX_NEIGHBOR_LIST_ITEM_COUNT 50
BUILD_ASSERT(CONFIG_NET_MGMT_EVENT_INFO_DEFAULT_DATA_SIZE >=
		     (DECT_L2_MAX_NEIGHBOR_LIST_ITEM_COUNT * sizeof(uint32_t) + sizeof(uint8_t) +
		      sizeof(int)),
	     "CONFIG_NET_MGMT_EVENT_INFO_DEFAULT_DATA_SIZE too small");

/** @brief neighbor list event */
struct dect_neighbor_list_evt {
	enum dect_status_values status;
	uint8_t neighbor_count;
	uint32_t neighbor_long_rd_ids[DECT_L2_MAX_NEIGHBOR_LIST_ITEM_COUNT];
};

/** @brief neighbor info event */
struct dect_neighbor_info_evt {
	enum dect_status_values status;
	uint32_t long_rd_id;

	/** Neighbor status information, valid only if status == DECT_MAC_STATUS_OK */

	/** True if neighbor is associated */
	bool associated;
	/** True if neighbor operates in FT mode. */
	bool ft_mode;

	/**
	 * PT-mode neighbor: cluster channel
	 * FT-mode neighbor: Last known operating channel of it's cluster
	 */
	uint16_t channel;

	/** Network ID */
	uint32_t network_id;

	/** Time in ms since neighbor is last seen */
	uint32_t time_since_last_rx_ms;

	/** Last received signal information */
	struct dect_rx_signal_info last_rx_signal_info;
	/**
	 * Average transmission power used by neighbor (PHY type 1 transmissions).
	 * Value in dB.
	 *
	 * Valid for FT-mode neighbors only.
	 */
	int8_t beacon_average_rx_txpower;
	/**
	 * Average received signal strength indicator (RSSI-2) of PDC
	 * (PHY type 1 transmissions).
	 * Value in dBm.
	 *
	 * Valid for FT-mode neighbors only.
	 */
	int16_t beacon_average_rx_rssi_2;
	/**
	 * Average received signal to noise indicator (SNR) of PDC
	 * (PHY type 1 transmissions).
	 * Value in dB.
	 *
	 * Valid for FT-mode neighbors only.
	 */
	int8_t beacon_average_rx_snr;

	struct dect_neighbor_status_info {
		/**
		 * Number of missed cluster beacons
		 *
		 * Valid for FT-mode neighbors only.
		 */
		uint8_t total_missed_cluster_beacons;
		/**
		 * Current number of consecutive missed cluster beacons
		 *
		 * Valid for FT-mode neighbors only.
		 */
		uint8_t current_consecutive_missed_cluster_beacons;
		/**
		 * Number of received paging IEs
		 *
		 * Valid for FT-mode neighbors only.
		 */
		uint8_t num_rx_paging;
		/**
		 * Average MCS used (PHY type 2 transmissions)
		 *
		 * Note: Statistics are gathered only from PHY type 2 transmissions
		 * Beacon messages are excluded.
		 */
		uint8_t average_tx_mcs;
		/**
		 * Average transmission power (PHY type 2 transmissions).
		 * Value in dB.
		 *
		 * Note: Statistics are gathered only from PHY type 2 transmissions
		 * Beacon messages are excluded.
		 */
		int8_t average_tx_txpower;
		/**
		 * Average MCS used by neighbor (PHY type 2 transmissions)
		 *
		 * Note: Statistics are gathered only from PHY type 2 transmissions
		 * Beacon messages are excluded.
		 */
		uint8_t average_rx_mcs;
		/**
		 * Average transmission power used by neighbor (PHY type 2 transmissions).
		 * Value in dB.
		 *
		 * Note: Statistics are gathered only from PHY type 2 transmissions
		 * Beacon messages are excluded.
		 */
		int8_t average_rx_txpower;
		/**
		 * Average received signal to noise indicator (SNR) of PDC
		 * (PHY type 2 transmissions).
		 * Value in dB.
		 *
		 * Note: Statistics are gathered only from PHY type 2 transmissions
		 * Beacon messages are excluded.
		 */
		int8_t average_rx_snr;
		/**
		 * Average received signal strength indicator (RSSI-2) of PDC
		 * (PHY type 2 transmissions).
		 * Value in dBm.
		 *
		 * Note: Statistics are gathered only from PHY type 2 transmissions
		 * Beacon messages are excluded.
		 */
		int16_t average_rx_rssi_2;
		/**
		 * Number of RACH transmissions attempts.
		 *
		 * Note: Statistics are gathered only from PHY type 2 transmissions
		 * Beacon messages are excluded.
		 */
		uint16_t num_tx_attempts;
		/**
		 * Number of LBT failures.
		 *
		 * Note: Statistics are gathered only from PHY type 2 transmissions
		 * Beacon messages are excluded.
		 */
		uint16_t num_lbt_failures;
		/**
		 * Number of received RACH PDCs.
		 *
		 * Note: Statistics are gathered only from PHY type 2 transmissions
		 * Beacon messages are excluded.
		 */
		uint16_t num_rx_pdc;
		/**
		 * Number of RACH PDC CRC failures.
		 *
		 * Note: Statistics are gathered only from PHY type 2 transmissions
		 * Beacon messages are excluded.
		 */
		uint16_t num_rx_pdc_crc_failures;
		/**
		 * Number of missed RACH response.
		 *
		 * Note: Statistics are gathered only from PHY type 2 transmissions
		 * Beacon messages are excluded.
		 */
		uint16_t num_no_response;
		/**
		 * Number of received HARQ ACKs.
		 *
		 * Note: Statistics are gathered only from PHY type 2 transmissions
		 * Beacon messages are excluded.
		 */
		uint16_t num_harq_ack;
		/**
		 * Number of received HARQ NACKs.
		 *
		 * Note: Statistics are gathered only from PHY type 2 transmissions
		 * Beacon messages are excluded.
		 */
		uint16_t num_harq_nack;
		/**
		 * Number of transmitted ARQ retransmissions
		 *
		 * Note: Statistics are gathered only from PHY type 2 transmissions
		 * Beacon messages are excluded.
		 */
		uint16_t num_arq_retx;
		/**
		 * Data inactive time in ms.
		 *
		 * Note: Statistics are gathered only from PHY type 2 transmissions
		 * Beacon messages are excluded.
		 */
		uint32_t inactive_time_ms;
	} status_info;
};

/** @brief cluster info event */
struct dect_cluster_info_evt {
	enum dect_status_values status;

	/** Cluster status information, valid only if status == DECT_MAC_STATUS_OK */
	struct dect_cluster_status_info {
		/** Number of received Association requests. */
		uint16_t num_association_requests;
		/** Number of failed Associations, includes rejections. */
		uint16_t num_association_failures;
		/** Number of neighbors. */
		uint16_t num_neighbors;
		/** Number of neighbors in FTPT-mode. */
		uint16_t num_ftpt_neighbors;
		/** Number of received RACH PDCs. */
		uint32_t num_rach_rx_pdc;
		/** Number of RACH PCC CRC failures. */
		uint32_t num_rach_rx_pcc_crc_failures;
		/** Current RSSI scan result of cluster channel */
		struct dect_rssi_scan_result_data rssi_result;
	} status_info;
};

struct dect_common_resp_evt {
	enum dect_status_values status;
};

/**************************************************************************************************/

enum dect_device_type {
	DECT_DEVICE_TYPE_FT,
	DECT_DEVICE_TYPE_PT,
};

/* Note: If writing just specific scopes of the settings, then all of the setting in given scope
 * need to be filled.
 */
enum dect_settings_cmd_params_write_scope {
	DECT_SETTINGS_WRITE_SCOPE_ALL = 0xFFFF,
	DECT_SETTINGS_WRITE_SCOPE_AUTO_START = 0x0001,
	DECT_SETTINGS_WRITE_SCOPE_REGION = 0x0002,
	DECT_SETTINGS_WRITE_SCOPE_DEVICE_TYPE = 0x0004,
	DECT_SETTINGS_WRITE_SCOPE_IDENTITIES = 0x0008,
	DECT_SETTINGS_WRITE_SCOPE_TX = 0x0010,
	DECT_SETTINGS_WRITE_SCOPE_POWER_SAVE = 0x0020,
	DECT_SETTINGS_WRITE_SCOPE_BAND_NBR = 0x0040,
	DECT_SETTINGS_WRITE_SCOPE_RSSI_SCAN = 0x0080,
	DECT_SETTINGS_WRITE_SCOPE_CLUSTER = 0x0100,
	DECT_SETTINGS_WRITE_SCOPE_NW_BEACON = 0x0200,
	DECT_SETTINGS_WRITE_SCOPE_ASSOCIATION = 0x0400,
	DECT_SETTINGS_WRITE_SCOPE_NETWORK_JOIN = 0x0800,
	DECT_SETTINGS_WRITE_SCOPE_SECURITY_CONFIGURATION = 0x1000,
};

struct dect_settings_cmd_params {
	bool reset_to_driver_defaults;
	uint16_t write_scope_bitmap; /* bitmask of dect_settings_cmd_params_write_scope */
};

/**************************************************************************************************/

#define DECT_MAC_NW_BEACON_CHANNEL_NOT_USED UINT16_MAX

/** FT device: cluster settings */
struct dect_settings_cluster {
	/** TX power used for cluster and network beacon transmission */
	int8_t max_beacon_tx_power_dbm;
	/** Cluster Max TX power */
	int8_t max_cluster_power_dbm;
	/** Cluster beacon send periodicity */
	enum dect_cluster_beacon_period beacon_period;
	/** Maximum number of associations to accept. */
	uint16_t max_num_neighbors;
	/** Neighbor inactivity timer that triggers association releasing.
	 *  Time in milliseconds, Use 0 to disable timer.
	 */
	uint32_t neighbor_inactivity_disconnect_timer_ms;

	/** Threshold when an operating channel load (=busy percentage) is so high that
	 *  the RD should start Operating Channel(s) and Subslot(s) selection.
	 *  Setting to zero disables the feature and the RD will not perform any
	 *  channel reselection automatically.
	 */
	uint8_t channel_loaded_percent;
};

struct dect_settings_network_beacon {
	uint16_t channel;
	enum dect_nw_beacon_period beacon_period;
};

struct dect_settings_association {
	/**
	 * Consecutive cluster beacon misses before
	 * triggering procedures for a release of association.
	 */
	uint8_t max_beacon_rx_failures;
};

struct dect_settings_auto_start {
	bool activate; /* if true, then auto-activate DECT NR+ stack on bootup */
};

#define DECT_SETT_NETWORK_JOIN_TARGET_FT_ANY 0
struct dect_settings_network_join {
	/** Target FT Long Radio Device ID */
	uint32_t target_ft_long_rd_id;
};

struct dect_settings_rssi_scan {
	uint8_t scan_suitable_percent;

	uint32_t time_per_channel_ms;

	int32_t free_threshold_dbm; /* if equal or less considered as free */
	/* rssi_scanning_busy_threshold <= possible < rssi_scanning_free_threshold*/
	int32_t busy_threshold_dbm; /* if higher considered as busy */
};

enum dect_settings_region {
	DECT_SETTINGS_REGION_EU = 0,
	DECT_SETTINGS_REGION_US = 1,
	DECT_SETTINGS_REGION_GLOBAL,
};
struct dect_settings_identities {
	/** Network ID */
	uint32_t network_id;

	/** Transmitter Long Radio Device ID */
	uint32_t transmitter_long_rd_id;
};

struct dect_settings_common_tx {
	/** Maximum TX power. */
	int8_t max_power_dbm;

	/** Maximum MCS . */
	uint8_t max_mcs;
};

/**************************************************************************************************/

#define DECT_MAC_INTEGRITY_KEY_LENGTH 16
#define DECT_MAC_CIPHER_KEY_LENGTH    16

/**
 * @brief Security mode.
 */
enum dect_mac_security_mode {
	/** None */
	DECT_MAC_SECURITY_MODE_NONE = 0,
	/** Mode 1 */
	DECT_MAC_SECURITY_MODE_1 = 1,
};

/**
 * @brief Security configuration.
 */
struct dect_settings_security_conf {
	/** Security mode */
	enum dect_mac_security_mode mode;

	/** Security keys.*/
	uint8_t integrity_key[DECT_MAC_INTEGRITY_KEY_LENGTH];
	uint8_t cipher_key[DECT_MAC_CIPHER_KEY_LENGTH];
};

/**************************************************************************************************/

/**
 * @brief DECT NR+ Settings.
 */

struct dect_settings {
	/** Command params */
	struct dect_settings_cmd_params cmd_params;

	/* Auto-start settings */
	struct dect_settings_auto_start auto_start;

	/** Region */
	enum dect_settings_region region;

	/** DECT device type */
	enum dect_device_type device_type;

	/** DECT identifiers */
	struct dect_settings_identities identities;

	/** Common TX settings */
	struct dect_settings_common_tx tx;

	/** Power save. */
	bool power_save;

	/** Band */
	uint8_t band_nbr;

	/** RSSI scan */
	struct dect_settings_rssi_scan rssi_scan;

	/** Cluster settings */
	struct dect_settings_cluster cluster;

	/** Network beacon settings */
	struct dect_settings_network_beacon nw_beacon;

	/** Association settings */
	struct dect_settings_association association;

	/** Network join settings */
	struct dect_settings_network_join network_join;

	/** Security configuration */
	struct dect_settings_security_conf sec_conf;
};

struct dect_association_data {
	uint32_t long_rd_id;

	struct in6_addr local_ipv6_addr;

	bool global_ipv6_addr_set;
	struct in6_addr global_ipv6_addr;
};

struct dect_status_info {

	/** true when dect nr+ stack in modem is activated */
	bool mdm_activated;

	/** Cluster information */
	bool cluster_running;
	uint16_t cluster_channel;

	/** Network beacon information */
	bool nw_beacon_running;

	/** Parent info */
	uint8_t parent_count;
	struct dect_association_data parent_associations[1];

	/** Associated children */
	uint8_t child_count;
	struct dect_association_data
		child_associations[CONFIG_DECT_NRP_MAC_CLUSTER_MAX_CHILD_ASSOCIATION_COUNT];

	/** Border gateway and sink info */
	struct net_if *br_net_iface;
	bool br_global_ipv6_addr_prefix_set;
	struct in6_addr br_global_ipv6_addr_prefix;
	int br_global_ipv6_addr_prefix_len;

	/** Modem firmware version */
	char fw_version_str[100];
};

/** DECT NR+ HAL API */
struct dect_nrp_hal_api {
	/**
	 * The net_if_api must be placed in first position in this
	 * struct so that we are compatible with network interface API.
	 */
	struct net_if_api iface_api;

	/** Read dect status information (syncronous/blocking)
	 *
	 * @param dev Pointer to the device structure for the driver instance.
	 * @param status_info Status info if successful
	 * @return 0 if ok, < 0 if error
	 */
	int (*status_info_get)(const struct device *dev, struct dect_status_info *status_info_out);

	/** Read dect settings (syncronous/blocking)
	 *
	 * @param dev Pointer to the device structure for the driver instance.
	 * @param settings Settings if successful
	 * @return 0 if ok, < 0 if error
	 */
	int (*settings_read)(const struct device *dev, struct dect_settings *settings_out);

	/** Write dect settings (syncronous/blocking)
	 *
	 * @param dev Pointer to the device structure for the driver instance.
	 * @param settings Settings to write
	 * @return 0 if ok, < 0 if error
	 */
	int (*settings_write)(const struct device *dev, const struct dect_settings *settings_in);

	/** Activate dect nr+ stack
	 *
	 * @param dev Pointer to the device structure for the driver instance.
	 * @return 0 if ok, < 0 if error
	 */
	int (*activate_req)(const struct device *dev);

	/** De-activate dect nr+ stack
	 *
	 * @param dev Pointer to the device structure for the driver instance.
	 * @return 0 if ok, < 0 if error
	 */
	int (*deactivate_req)(const struct device *dev);

	/** Execute a RSSI scan.
	 *
	 * * @param dev Pointer to the device structure for the driver instance.
	 * * @param params Scan parameters
	 */
	int (*rssi_scan)(const struct device *dev, struct dect_rssi_scan_params *params);

	/** Scan for dect beacons
	 *
	 * @param dev Pointer to the device structure for the driver instance.
	 * @param params Scan parameters
	 * @param cb Callback to be called for each result
	 *           cb parameter is the cb that should be called for each
	 *           result by the driver.
	 *
	 * @return 0 if ok, < 0 if error
	 */
	int (*scan)(const struct device *dev, struct dect_scan_params *params,
		    dect_scan_result_cb_t cb);

	/** Associate with FT
	 *
	 * @param dev Pointer to the device structure for the driver instance.
	 * @param params Associate parameters
	 *
	 * @return 0 if ok, < 0 if error
	 */
	int (*associate_req)(const struct device *dev, struct dect_associate_req_params *params);

	/** Release existing association
	 *
	 * @param dev Pointer to the device structure for the driver instance.
	 * @param params Association Release parameters
	 *
	 * @return 0 if ok, < 0 if error
	 */
	int (*associate_release)(const struct device *dev,
				 struct dect_associate_rel_params *params);

	/** FT: Start a cluster
	 *
	 * @param dev Pointer to the device structure for the driver instance.
	 * @param params Cluster start parameters
	 *
	 * @return 0 if ok, < 0 if error
	 */
	int (*cluster_start_req)(const struct device *dev,
				 struct dect_cluster_start_req_params *params);

	/** FT: Reconfigure a cluster
	 *
	 * @param dev Pointer to the device structure for the driver instance.
	 * @param params Cluster reconfiguration parameters
	 *
	 * @return 0 if ok, < 0 if error
	 */
	int (*cluster_reconfig_req)(const struct device *dev,
				    struct dect_cluster_reconfig_req_params *params);

	/** FT: Stop a cluster
	 *
	 * @param dev Pointer to the device structure for the driver instance.
	 * @param params Cluster stop parameters
	 *
	 * @return 0 if ok, < 0 if error
	 */
	int (*cluster_stop_req)(const struct device *dev,
				struct dect_cluster_stop_req_params *params);

	/** FT: Start a network beacon
	 *
	 * @param dev Pointer to the device structure for the driver instance.
	 * @param params Parameters
	 *
	 * @return 0 if ok, < 0 if error
	 */
	int (*nw_beacon_start_req)(const struct device *dev,
				   struct dect_nw_beacon_start_req_params *params);

	/** FT: Stop a network beacon
	 *
	 * @param dev Pointer to the device structure for the driver instance.
	 * @param params Parameters
	 *
	 * @return 0 if ok, < 0 if error
	 */
	int (*nw_beacon_stop_req)(const struct device *dev,
				  struct dect_nw_beacon_stop_req_params *params);

	/** Send a packet */
	int (*send)(const struct device *dev, struct net_pkt *pkt);

	/** Request neighbor list */
	int (*neighbor_list_req)(const struct device *dev);

	/** Request neighbor info */
	int (*neighbor_info_req)(const struct device *dev,
				 struct dect_neighbor_info_req_params *params);
	/** Request cluster info */
	int (*cluster_info_req)(const struct device *dev);

	/** FT: Request to create/enable a network as set in settings */
	int (*network_create_req)(const struct device *dev);

	/** FT: Request to remove/disable a network */
	int (*network_remove_req)(const struct device *dev);

	/** PT: Request to join a network as set in settings */
	int (*network_join_req)(const struct device *dev);

	/** PT: Request to unjoin from a network */
	int (*network_unjoin_req)(const struct device *dev);
};

/** DECT NR+ L2 context. */
struct dect_net_l2_context {
	/** L2 flags */
	enum net_l2_flags flags;

	/** DECT device type */
	enum dect_device_type device_type;

	/** Network ID */
	uint32_t network_id;

	/** Transmitter Long Radio Device ID */
	uint32_t transmitter_long_rd_id;

	/** Link layer address (in big endian) of this device */
	struct net_linkaddr linkaddr;
};

#define DECT_L2 DECT
NET_L2_DECLARE_PUBLIC(DECT_L2);

/* L2 context type to be used with NET_L2_GET_CTX_TYPE */
#define DECT_L2_CTX_TYPE struct dect_net_l2_context

/* Calls from driver to L2: */
void dect_net_l2_init(struct net_if *iface, struct dect_settings *driver_initial_settings);

/**
 * @brief Inform L2 that settings have been changed.
 *
 * @param iface Network interface
 */
void dect_net_l2_settings_changed(
	struct net_if *iface, struct dect_settings *driver_current_settings);

/**
 * @brief Inform L2 that association with a parent has been created. PT device.
 *
 * @param iface Network interface
 */
void dect_net_l2_parent_association_created(struct net_if *iface, uint32_t target_long_rd_id);

/**
 * @brief Inform L2 that association with a child has been created. FT device.
 *
 * @param iface Network interface
 */
void dect_net_l2_child_association_created(struct net_if *iface, uint32_t target_long_rd_id);

/**
 * @brief Inform L2 that association has been released.
 *
 * @param iface Network interface
 */
void dect_net_l2_association_removed(
	struct net_if *iface, uint32_t long_rd_id, enum dect_association_release_cause cause);

#endif /* ZEPHYR_INCLUDE_NET_NET_DECT_L2_H_ */

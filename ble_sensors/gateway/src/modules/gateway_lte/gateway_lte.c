#include "gateway_lte.h"
#include <modem/nrf_modem_lib.h>
#include <zephyr/logging/log.h>

#include <modem/nrf_modem_lib.h>
#include <modem/lte_lc.h>


LOG_MODULE_REGISTER(gateway_lte, LOG_LEVEL_INF);

static K_SEM_DEFINE(lte_connected, 0, 1);

rrc_state_t rrc_state = RRC_DISCONNECTED;

void rrc_state_change(rrc_state_t new_state)
{
    if (rrc_state != new_state) {
        LOG_WRN("%s => %s",
            (rrc_state == RRC_DISCONNECTED) ? "RRC_DISCONNECTED" :
            (rrc_state == RRC_CONNECTING) ? "RRC_CONNECTING" :
            (rrc_state == RRC_CONNECTED) ? "RRC_CONNECTED" :
            (rrc_state == RRC_IDLE) ? "RRC_IDLE" :
            "RRC_PSM",
            
            (new_state == RRC_DISCONNECTED) ? "RRC_DISCONNECTED" :
            (new_state == RRC_CONNECTING) ? "RRC_CONNECTING" :
            (new_state == RRC_CONNECTED) ? "RRC_CONNECTED" :
            (new_state == RRC_IDLE) ? "RRC_IDLE" :
            "RRC_PSM"
        );

        rrc_state = new_state;
    }
}

static void lte_handler(const struct lte_lc_evt *const evt)
{
	switch (evt->type) {
	case LTE_LC_EVT_NW_REG_STATUS:
		if (evt->nw_reg_status == LTE_LC_NW_REG_REGISTERED_HOME ||
            evt->nw_reg_status == LTE_LC_NW_REG_REGISTERED_ROAMING) {
            rrc_state_change(RRC_CONNECTED);
			LOG_INF("Network registration status: %s",
				evt->nw_reg_status == LTE_LC_NW_REG_REGISTERED_HOME ?
				"Connected - home network" : "Connected - roaming");
			k_sem_give(&lte_connected);

        } else if (evt->nw_reg_status == LTE_LC_NW_REG_SEARCHING ||
                   evt->nw_reg_status == LTE_LC_NW_REG_REGISTRATION_DENIED ||
                   evt->nw_reg_status == LTE_LC_NW_REG_UNKNOWN) {
            rrc_state_change(RRC_CONNECTING);
        } else {
            rrc_state_change(RRC_DISCONNECTED);
            LOG_ERR("Network Disconnected.\n");
        }
        break;

	case LTE_LC_EVT_PSM_UPDATE:
		LOG_INF("PSM parameter update: TAU: %d, Active time: %d",
			evt->psm_cfg.tau, evt->psm_cfg.active_time);
		break;

	#if defined(CONFIG_LTE_LC_EDRX_MODULE)
	case LTE_LC_EVT_EDRX_UPDATE: {
		char log_buf[60];
		ssize_t len;

		len = snprintf(log_buf, sizeof(log_buf),
			       "eDRX parameter update: eDRX: %f, PTW: %f",
			       evt->edrx_cfg.edrx, evt->edrx_cfg.ptw);
		if (len > 0) {
			LOG_INF("%s\n", log_buf);
		}
		break;
	#endif
	
	
	case LTE_LC_EVT_RRC_UPDATE:
        if (evt->rrc_mode == LTE_LC_RRC_MODE_CONNECTED) {
            rrc_state_change(RRC_CONNECTED);
        } else if (evt->rrc_mode == LTE_LC_RRC_MODE_IDLE && rrc_state == RRC_CONNECTED) {
            rrc_state_change(RRC_IDLE);
        }
        break;


	case LTE_LC_EVT_MODEM_SLEEP_ENTER:
		if (rrc_state != RRC_DISCONNECTED) {
			rrc_state_change(RRC_PSM);
		}
		break;

	case LTE_LC_EVT_CELL_UPDATE:
		LOG_INF("LTE cell changed: Cell ID: %d, Tracking area: %d",
		       evt->cell.id, evt->cell.tac);
		break;
	default:
		break;
	}
}


int gateway_lte_init(void)
{
	int err;

	LOG_INF("Initializing modem library");
	err = nrf_modem_lib_init();
	if (err) {
		LOG_ERR("Failed to initialize the modem library, error: %d", err);
		return err;
	}

	err = lte_lc_psm_req(true);
	if (err)
		{
			LOG_ERR("lte_lc_psm_req, error: %d", err);
		}
	
	LOG_INF("Connecting to LTE network");
	err = lte_lc_connect_async(lte_handler);
	if (err) {
		LOG_ERR("Error in lte_lc_connect_async, error: %d", err);
		return err;
	}

	k_sem_take(&lte_connected, K_FOREVER);
	LOG_INF("Connected to LTE network");

	return 0;
}
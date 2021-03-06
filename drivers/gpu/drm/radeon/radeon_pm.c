/*
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Authors: Rafał Miłecki <zajec5@gmail.com>
 *          Alex Deucher <alexdeucher@gmail.com>
 */
#include "drmP.h"
#include "radeon.h"
#include "avivod.h"
#ifdef CONFIG_ACPI
#include <linux/acpi.h>
#endif
#include <linux/power_supply.h>

#define RADEON_IDLE_LOOP_MS 100
#define RADEON_RECLOCK_DELAY_MS 200
#define RADEON_WAIT_VBLANK_TIMEOUT 200
#define RADEON_WAIT_IDLE_TIMEOUT 200

static void radeon_dynpm_idle_work_handler(struct work_struct *work);
static int radeon_debugfs_pm_init(struct radeon_device *rdev);
static bool radeon_pm_in_vbl(struct radeon_device *rdev);
static bool radeon_pm_debug_check_in_vbl(struct radeon_device *rdev, bool finish);
static void radeon_pm_update_profile(struct radeon_device *rdev);
static void radeon_pm_set_clocks(struct radeon_device *rdev);

#define ACPI_AC_CLASS           "ac_adapter"

#ifdef CONFIG_ACPI
static int radeon_acpi_event(struct notifier_block *nb,
			     unsigned long val,
			     void *data)
{
	struct radeon_device *rdev = container_of(nb, struct radeon_device, acpi_nb);
	struct acpi_bus_event *entry = (struct acpi_bus_event *)data;

	if (strcmp(entry->device_class, ACPI_AC_CLASS) == 0) {
		if (power_supply_is_system_supplied() > 0)
			DRM_DEBUG("pm: AC\n");
		else
			DRM_DEBUG("pm: DC\n");

		if (rdev->pm.pm_method == PM_METHOD_PROFILE) {
			if (rdev->pm.profile == PM_PROFILE_AUTO) {
				mutex_lock(&rdev->pm.mutex);
				radeon_pm_update_profile(rdev);
				radeon_pm_set_clocks(rdev);
				mutex_unlock(&rdev->pm.mutex);
			}
		}
	}

	return NOTIFY_OK;
}
#endif

static void radeon_pm_update_profile(struct radeon_device *rdev)
{
	switch (rdev->pm.profile) {
	case PM_PROFILE_DEFAULT:
		rdev->pm.profile_index = PM_PROFILE_DEFAULT_IDX;
		break;
	case PM_PROFILE_AUTO:
		if (power_supply_is_system_supplied() > 0) {
			if (rdev->pm.active_crtc_count > 1)
				rdev->pm.profile_index = PM_PROFILE_HIGH_MH_IDX;
			else
				rdev->pm.profile_index = PM_PROFILE_HIGH_SH_IDX;
		} else {
			if (rdev->pm.active_crtc_count > 1)
				rdev->pm.profile_index = PM_PROFILE_LOW_MH_IDX;
			else
				rdev->pm.profile_index = PM_PROFILE_LOW_SH_IDX;
		}
		break;
	case PM_PROFILE_LOW:
		if (rdev->pm.active_crtc_count > 1)
			rdev->pm.profile_index = PM_PROFILE_LOW_MH_IDX;
		else
			rdev->pm.profile_index = PM_PROFILE_LOW_SH_IDX;
		break;
	case PM_PROFILE_HIGH:
		if (rdev->pm.active_crtc_count > 1)
			rdev->pm.profile_index = PM_PROFILE_HIGH_MH_IDX;
		else
			rdev->pm.profile_index = PM_PROFILE_HIGH_SH_IDX;
		break;
	}

	if (rdev->pm.active_crtc_count == 0) {
		rdev->pm.requested_power_state_index =
			rdev->pm.profiles[rdev->pm.profile_index].dpms_off_ps_idx;
		rdev->pm.requested_clock_mode_index =
			rdev->pm.profiles[rdev->pm.profile_index].dpms_off_cm_idx;
	} else {
		rdev->pm.requested_power_state_index =
			rdev->pm.profiles[rdev->pm.profile_index].dpms_on_ps_idx;
		rdev->pm.requested_clock_mode_index =
			rdev->pm.profiles[rdev->pm.profile_index].dpms_on_cm_idx;
	}
}

static void radeon_unmap_vram_bos(struct radeon_device *rdev)
{
	struct radeon_bo *bo, *n;

	if (list_empty(&rdev->gem.objects))
		return;

	list_for_each_entry_safe(bo, n, &rdev->gem.objects, list) {
		if (bo->tbo.mem.mem_type == TTM_PL_VRAM)
			ttm_bo_unmap_virtual(&bo->tbo);
	}

	if (rdev->gart.table.vram.robj)
		ttm_bo_unmap_virtual(&rdev->gart.table.vram.robj->tbo);

	if (rdev->stollen_vga_memory)
		ttm_bo_unmap_virtual(&rdev->stollen_vga_memory->tbo);

	if (rdev->r600_blit.shader_obj)
		ttm_bo_unmap_virtual(&rdev->r600_blit.shader_obj->tbo);
}

static void radeon_sync_with_vblank(struct radeon_device *rdev)
{
	if (rdev->pm.active_crtcs) {
		rdev->pm.vblank_sync = false;
		wait_event_timeout(
			rdev->irq.vblank_queue, rdev->pm.vblank_sync,
			msecs_to_jiffies(RADEON_WAIT_VBLANK_TIMEOUT));
	}
}

static void radeon_set_power_state(struct radeon_device *rdev)
{
	u32 sclk, mclk;
	bool misc_after = false;

	if ((rdev->pm.requested_clock_mode_index == rdev->pm.current_clock_mode_index) &&
	    (rdev->pm.requested_power_state_index == rdev->pm.current_power_state_index))
		return;

	if (radeon_gui_idle(rdev)) {
		sclk = rdev->pm.power_state[rdev->pm.requested_power_state_index].
			clock_info[rdev->pm.requested_clock_mode_index].sclk;
		if (sclk > rdev->clock.default_sclk)
			sclk = rdev->clock.default_sclk;

		mclk = rdev->pm.power_state[rdev->pm.requested_power_state_index].
			clock_info[rdev->pm.requested_clock_mode_index].mclk;
		if (mclk > rdev->clock.default_mclk)
			mclk = rdev->clock.default_mclk;

		/* upvolt before raising clocks, downvolt after lowering clocks */
		if (sclk < rdev->pm.current_sclk)
			misc_after = true;

		radeon_sync_with_vblank(rdev);

		if (rdev->pm.pm_method == PM_METHOD_DYNPM) {
			if (!radeon_pm_in_vbl(rdev))
				return;
		}

		radeon_pm_prepare(rdev);

		if (!misc_after)
			/* voltage, pcie lanes, etc.*/
			radeon_pm_misc(rdev);

		/* set engine clock */
		if (sclk != rdev->pm.current_sclk) {
			radeon_pm_debug_check_in_vbl(rdev, false);
			radeon_set_engine_clock(rdev, sclk);
			radeon_pm_debug_check_in_vbl(rdev, true);
			rdev->pm.current_sclk = sclk;
			DRM_DEBUG("Setting: e: %d\n", sclk);
		}

		/* set memory clock */
		if (rdev->asic->set_memory_clock && (mclk != rdev->pm.current_mclk)) {
			radeon_pm_debug_check_in_vbl(rdev, false);
			radeon_set_memory_clock(rdev, mclk);
			radeon_pm_debug_check_in_vbl(rdev, true);
			rdev->pm.current_mclk = mclk;
			DRM_DEBUG("Setting: m: %d\n", mclk);
		}

		if (misc_after)
			/* voltage, pcie lanes, etc.*/
			radeon_pm_misc(rdev);

		radeon_pm_finish(rdev);

		rdev->pm.current_power_state_index = rdev->pm.requested_power_state_index;
		rdev->pm.current_clock_mode_index = rdev->pm.requested_clock_mode_index;
	} else
		DRM_DEBUG("pm: GUI not idle!!!\n");
}

static void radeon_pm_set_clocks(struct radeon_device *rdev)
{
	int i;

	mutex_lock(&rdev->ddev->struct_mutex);
	mutex_lock(&rdev->vram_mutex);
	mutex_lock(&rdev->cp.mutex);

	/* gui idle int has issues on older chips it seems */
	if (rdev->family >= CHIP_R600) {
		if (rdev->irq.installed) {
			/* wait for GPU idle */
			rdev->pm.gui_idle = false;
			rdev->irq.gui_idle = true;
			radeon_irq_set(rdev);
			wait_event_interruptible_timeout(
				rdev->irq.idle_queue, rdev->pm.gui_idle,
				msecs_to_jiffies(RADEON_WAIT_IDLE_TIMEOUT));
			rdev->irq.gui_idle = false;
			radeon_irq_set(rdev);
		}
	} else {
		if (rdev->cp.ready) {
			struct radeon_fence *fence;
			radeon_ring_alloc(rdev, 64);
			radeon_fence_create(rdev, &fence);
			radeon_fence_emit(rdev, fence);
			radeon_ring_commit(rdev);
			radeon_fence_wait(fence, false);
			radeon_fence_unref(&fence);
		}
	}
	radeon_unmap_vram_bos(rdev);

	if (rdev->irq.installed) {
		for (i = 0; i < rdev->num_crtc; i++) {
			if (rdev->pm.active_crtcs & (1 << i)) {
				rdev->pm.req_vblank |= (1 << i);
				drm_vblank_get(rdev->ddev, i);
			}
		}
	}

	radeon_set_power_state(rdev);

	if (rdev->irq.installed) {
		for (i = 0; i < rdev->num_crtc; i++) {
			if (rdev->pm.req_vblank & (1 << i)) {
				rdev->pm.req_vblank &= ~(1 << i);
				drm_vblank_put(rdev->ddev, i);
			}
		}
	}

	/* update display watermarks based on new power state */
	radeon_update_bandwidth_info(rdev);
	if (rdev->pm.active_crtc_count)
		radeon_bandwidth_update(rdev);

	rdev->pm.dynpm_planned_action = DYNPM_ACTION_NONE;

	mutex_unlock(&rdev->cp.mutex);
	mutex_unlock(&rdev->vram_mutex);
	mutex_unlock(&rdev->ddev->struct_mutex);
}

static ssize_t radeon_get_pm_profile(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{
	struct drm_device *ddev = pci_get_drvdata(to_pci_dev(dev));
	struct radeon_device *rdev = ddev->dev_private;
	int cp = rdev->pm.profile;

	return snprintf(buf, PAGE_SIZE, "%s\n",
			(cp == PM_PROFILE_AUTO) ? "auto" :
			(cp == PM_PROFILE_LOW) ? "low" :
			(cp == PM_PROFILE_HIGH) ? "high" : "default");
}

static ssize_t radeon_set_pm_profile(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf,
				     size_t count)
{
	struct drm_device *ddev = pci_get_drvdata(to_pci_dev(dev));
	struct radeon_device *rdev = ddev->dev_private;

	mutex_lock(&rdev->pm.mutex);
	if (rdev->pm.pm_method == PM_METHOD_PROFILE) {
		if (strncmp("default", buf, strlen("default")) == 0)
			rdev->pm.profile = PM_PROFILE_DEFAULT;
		else if (strncmp("auto", buf, strlen("auto")) == 0)
			rdev->pm.profile = PM_PROFILE_AUTO;
		else if (strncmp("low", buf, strlen("low")) == 0)
			rdev->pm.profile = PM_PROFILE_LOW;
		else if (strncmp("high", buf, strlen("high")) == 0)
			rdev->pm.profile = PM_PROFILE_HIGH;
		else {
			DRM_ERROR("invalid power profile!\n");
			goto fail;
		}
		radeon_pm_update_profile(rdev);
		radeon_pm_set_clocks(rdev);
	}
fail:
	mutex_unlock(&rdev->pm.mutex);

	return count;
}

static ssize_t radeon_get_pm_method(struct device *dev,
				    struct device_attribute *attr,
				    char *buf)
{
	struct drm_device *ddev = pci_get_drvdata(to_pci_dev(dev));
	struct radeon_device *rdev = ddev->dev_private;
	int pm = rdev->pm.pm_method;

	return snprintf(buf, PAGE_SIZE, "%s\n",
			(pm == PM_METHOD_DYNPM) ? "dynpm" : "profile");
}

static ssize_t radeon_set_pm_method(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf,
				    size_t count)
{
	struct drm_device *ddev = pci_get_drvdata(to_pci_dev(dev));
	struct radeon_device *rdev = ddev->dev_private;


	if (strncmp("dynpm", buf, strlen("dynpm")) == 0) {
		mutex_lock(&rdev->pm.mutex);
		rdev->pm.pm_method = PM_METHOD_DYNPM;
		rdev->pm.dynpm_state = DYNPM_STATE_PAUSED;
		rdev->pm.dynpm_planned_action = DYNPM_ACTION_DEFAULT;
		mutex_unlock(&rdev->pm.mutex);
	} else if (strncmp("profile", buf, strlen("profile")) == 0) {
		mutex_lock(&rdev->pm.mutex);
		rdev->pm.pm_method = PM_METHOD_PROFILE;
		/* disable dynpm */
		rdev->pm.dynpm_state = DYNPM_STATE_DISABLED;
		rdev->pm.dynpm_planned_action = DYNPM_ACTION_NONE;
		cancel_delayed_work(&rdev->pm.dynpm_idle_work);
		mutex_unlock(&rdev->pm.mutex);
	} else {
		DRM_ERROR("invalid power method!\n");
		goto fail;
	}
	radeon_pm_compute_clocks(rdev);
fail:
	return count;
}

static DEVICE_ATTR(power_profile, S_IRUGO | S_IWUSR, radeon_get_pm_profile, radeon_set_pm_profile);
static DEVICE_ATTR(power_method, S_IRUGO | S_IWUSR, radeon_get_pm_method, radeon_set_pm_method);

void radeon_pm_suspend(struct radeon_device *rdev)
{
	mutex_lock(&rdev->pm.mutex);
	cancel_delayed_work(&rdev->pm.dynpm_idle_work);
	rdev->pm.current_power_state_index = -1;
	rdev->pm.current_clock_mode_index = -1;
	rdev->pm.current_sclk = 0;
	rdev->pm.current_mclk = 0;
	mutex_unlock(&rdev->pm.mutex);
}

void radeon_pm_resume(struct radeon_device *rdev)
{
	radeon_pm_compute_clocks(rdev);
}

int radeon_pm_init(struct radeon_device *rdev)
{
	int ret;
	/* default to profile method */
	rdev->pm.pm_method = PM_METHOD_PROFILE;
	rdev->pm.dynpm_state = DYNPM_STATE_DISABLED;
	rdev->pm.dynpm_planned_action = DYNPM_ACTION_NONE;
	rdev->pm.dynpm_can_upclock = true;
	rdev->pm.dynpm_can_downclock = true;
	rdev->pm.current_sclk = 0;
	rdev->pm.current_mclk = 0;

	if (rdev->bios) {
		if (rdev->is_atom_bios)
			radeon_atombios_get_power_modes(rdev);
		else
			radeon_combios_get_power_modes(rdev);
		radeon_pm_init_profile(rdev);
		rdev->pm.current_power_state_index = -1;
		rdev->pm.current_clock_mode_index = -1;
	}

	if (rdev->pm.num_power_states > 1) {
		if (rdev->pm.pm_method == PM_METHOD_PROFILE) {
			mutex_lock(&rdev->pm.mutex);
			rdev->pm.profile = PM_PROFILE_DEFAULT;
			radeon_pm_update_profile(rdev);
			radeon_pm_set_clocks(rdev);
			mutex_unlock(&rdev->pm.mutex);
		}

		/* where's the best place to put these? */
		ret = device_create_file(rdev->dev, &dev_attr_power_profile);
		if (ret)
			DRM_ERROR("failed to create device file for power profile\n");
		ret = device_create_file(rdev->dev, &dev_attr_power_method);
		if (ret)
			DRM_ERROR("failed to create device file for power method\n");

#ifdef CONFIG_ACPI
		rdev->acpi_nb.notifier_call = radeon_acpi_event;
		register_acpi_notifier(&rdev->acpi_nb);
#endif
		INIT_DELAYED_WORK(&rdev->pm.dynpm_idle_work, radeon_dynpm_idle_work_handler);

		if (radeon_debugfs_pm_init(rdev)) {
			DRM_ERROR("Failed to register debugfs file for PM!\n");
		}

		DRM_INFO("radeon: power management initialized\n");
	}

	return 0;
}

void radeon_pm_fini(struct radeon_device *rdev)
{
	if (rdev->pm.num_power_states > 1) {
		mutex_lock(&rdev->pm.mutex);
		if (rdev->pm.pm_method == PM_METHOD_PROFILE) {
			rdev->pm.profile = PM_PROFILE_DEFAULT;
			radeon_pm_update_profile(rdev);
			radeon_pm_set_clocks(rdev);
		} else if (rdev->pm.pm_method == PM_METHOD_DYNPM) {
			/* cancel work */
			cancel_delayed_work_sync(&rdev->pm.dynpm_idle_work);
			/* reset default clocks */
			rdev->pm.dynpm_state = DYNPM_STATE_DISABLED;
			rdev->pm.dynpm_planned_action = DYNPM_ACTION_DEFAULT;
			radeon_pm_set_clocks(rdev);
		}
		mutex_unlock(&rdev->pm.mutex);

		device_remove_file(rdev->dev, &dev_attr_power_profile);
		device_remove_file(rdev->dev, &dev_attr_power_method);
#ifdef CONFIG_ACPI
		unregister_acpi_notifier(&rdev->acpi_nb);
#endif
	}

	if (rdev->pm.i2c_bus)
		radeon_i2c_destroy(rdev->pm.i2c_bus);
}

void radeon_pm_compute_clocks(struct radeon_device *rdev)
{
	struct drm_device *ddev = rdev->ddev;
	struct drm_crtc *crtc;
	struct radeon_crtc *radeon_crtc;

	if (rdev->pm.num_power_states < 2)
		return;

	mutex_lock(&rdev->pm.mutex);

	rdev->pm.active_crtcs = 0;
	rdev->pm.active_crtc_count = 0;
	list_for_each_entry(crtc,
		&ddev->mode_config.crtc_list, head) {
		radeon_crtc = to_radeon_crtc(crtc);
		if (radeon_crtc->enabled) {
			rdev->pm.active_crtcs |= (1 << radeon_crtc->crtc_id);
			rdev->pm.active_crtc_count++;
		}
	}

	if (rdev->pm.pm_method == PM_METHOD_PROFILE) {
		radeon_pm_update_profile(rdev);
		radeon_pm_set_clocks(rdev);
	} else if (rdev->pm.pm_method == PM_METHOD_DYNPM) {
		if (rdev->pm.dynpm_state != DYNPM_STATE_DISABLED) {
			if (rdev->pm.active_crtc_count > 1) {
				if (rdev->pm.dynpm_state == DYNPM_STATE_ACTIVE) {
					cancel_delayed_work(&rdev->pm.dynpm_idle_work);

					rdev->pm.dynpm_state = DYNPM_STATE_PAUSED;
					rdev->pm.dynpm_planned_action = DYNPM_ACTION_DEFAULT;
					radeon_pm_get_dynpm_state(rdev);
					radeon_pm_set_clocks(rdev);

					DRM_DEBUG("radeon: dynamic power management deactivated\n");
				}
			} else if (rdev->pm.active_crtc_count == 1) {
				/* TODO: Increase clocks if needed for current mode */

				if (rdev->pm.dynpm_state == DYNPM_STATE_MINIMUM) {
					rdev->pm.dynpm_state = DYNPM_STATE_ACTIVE;
					rdev->pm.dynpm_planned_action = DYNPM_ACTION_UPCLOCK;
					radeon_pm_get_dynpm_state(rdev);
					radeon_pm_set_clocks(rdev);

					queue_delayed_work(rdev->wq, &rdev->pm.dynpm_idle_work,
							   msecs_to_jiffies(RADEON_IDLE_LOOP_MS));
				} else if (rdev->pm.dynpm_state == DYNPM_STATE_PAUSED) {
					rdev->pm.dynpm_state = DYNPM_STATE_ACTIVE;
					queue_delayed_work(rdev->wq, &rdev->pm.dynpm_idle_work,
							   msecs_to_jiffies(RADEON_IDLE_LOOP_MS));
					DRM_DEBUG("radeon: dynamic power management activated\n");
				}
			} else { /* count == 0 */
				if (rdev->pm.dynpm_state != DYNPM_STATE_MINIMUM) {
					cancel_delayed_work(&rdev->pm.dynpm_idle_work);

					rdev->pm.dynpm_state = DYNPM_STATE_MINIMUM;
					rdev->pm.dynpm_planned_action = DYNPM_ACTION_MINIMUM;
					radeon_pm_get_dynpm_state(rdev);
					radeon_pm_set_clocks(rdev);
				}
			}
		}
	}

	mutex_unlock(&rdev->pm.mutex);
}

static bool radeon_pm_in_vbl(struct radeon_device *rdev)
{
	u32 stat_crtc = 0, vbl = 0, position = 0;
	bool in_vbl = true;

	if (ASIC_IS_DCE4(rdev)) {
		if (rdev->pm.active_crtcs & (1 << 0)) {
			vbl = RREG32(EVERGREEN_CRTC_V_BLANK_START_END +
				     EVERGREEN_CRTC0_REGISTER_OFFSET) & 0xfff;
			position = RREG32(EVERGREEN_CRTC_STATUS_POSITION +
					  EVERGREEN_CRTC0_REGISTER_OFFSET) & 0xfff;
		}
		if (rdev->pm.active_crtcs & (1 << 1)) {
			vbl = RREG32(EVERGREEN_CRTC_V_BLANK_START_END +
				     EVERGREEN_CRTC1_REGISTER_OFFSET) & 0xfff;
			position = RREG32(EVERGREEN_CRTC_STATUS_POSITION +
					  EVERGREEN_CRTC1_REGISTER_OFFSET) & 0xfff;
		}
		if (rdev->pm.active_crtcs & (1 << 2)) {
			vbl = RREG32(EVERGREEN_CRTC_V_BLANK_START_END +
				     EVERGREEN_CRTC2_REGISTER_OFFSET) & 0xfff;
			position = RREG32(EVERGREEN_CRTC_STATUS_POSITION +
					  EVERGREEN_CRTC2_REGISTER_OFFSET) & 0xfff;
		}
		if (rdev->pm.active_crtcs & (1 << 3)) {
			vbl = RREG32(EVERGREEN_CRTC_V_BLANK_START_END +
				     EVERGREEN_CRTC3_REGISTER_OFFSET) & 0xfff;
			position = RREG32(EVERGREEN_CRTC_STATUS_POSITION +
					  EVERGREEN_CRTC3_REGISTER_OFFSET) & 0xfff;
		}
		if (rdev->pm.active_crtcs & (1 << 4)) {
			vbl = RREG32(EVERGREEN_CRTC_V_BLANK_START_END +
				     EVERGREEN_CRTC4_REGISTER_OFFSET) & 0xfff;
			position = RREG32(EVERGREEN_CRTC_STATUS_POSITION +
					  EVERGREEN_CRTC4_REGISTER_OFFSET) & 0xfff;
		}
		if (rdev->pm.active_crtcs & (1 << 5)) {
			vbl = RREG32(EVERGREEN_CRTC_V_BLANK_START_END +
				     EVERGREEN_CRTC5_REGISTER_OFFSET) & 0xfff;
			position = RREG32(EVERGREEN_CRTC_STATUS_POSITION +
					  EVERGREEN_CRTC5_REGISTER_OFFSET) & 0xfff;
		}
	} else if (ASIC_IS_AVIVO(rdev)) {
		if (rdev->pm.active_crtcs & (1 << 0)) {
			vbl = RREG32(AVIVO_D1CRTC_V_BLANK_START_END) & 0xfff;
			position = RREG32(AVIVO_D1CRTC_STATUS_POSITION) & 0xfff;
		}
		if (rdev->pm.active_crtcs & (1 << 1)) {
			vbl = RREG32(AVIVO_D2CRTC_V_BLANK_START_END) & 0xfff;
			position = RREG32(AVIVO_D2CRTC_STATUS_POSITION) & 0xfff;
		}
		if (position < vbl && position > 1)
			in_vbl = false;
	} else {
		if (rdev->pm.active_crtcs & (1 << 0)) {
			stat_crtc = RREG32(RADEON_CRTC_STATUS);
			if (!(stat_crtc & 1))
				in_vbl = false;
		}
		if (rdev->pm.active_crtcs & (1 << 1)) {
			stat_crtc = RREG32(RADEON_CRTC2_STATUS);
			if (!(stat_crtc & 1))
				in_vbl = false;
		}
	}

	if (position < vbl && position > 1)
		in_vbl = false;

	return in_vbl;
}

static bool radeon_pm_debug_check_in_vbl(struct radeon_device *rdev, bool finish)
{
	u32 stat_crtc = 0;
	bool in_vbl = radeon_pm_in_vbl(rdev);

	if (in_vbl == false)
		DRM_DEBUG("not in vbl for pm change %08x at %s\n", stat_crtc,
			 finish ? "exit" : "entry");
	return in_vbl;
}

static void radeon_dynpm_idle_work_handler(struct work_struct *work)
{
	struct radeon_device *rdev;
	int resched;
	rdev = container_of(work, struct radeon_device,
				pm.dynpm_idle_work.work);

	resched = ttm_bo_lock_delayed_workqueue(&rdev->mman.bdev);
	mutex_lock(&rdev->pm.mutex);
	if (rdev->pm.dynpm_state == DYNPM_STATE_ACTIVE) {
		unsigned long irq_flags;
		int not_processed = 0;

		read_lock_irqsave(&rdev->fence_drv.lock, irq_flags);
		if (!list_empty(&rdev->fence_drv.emited)) {
			struct list_head *ptr;
			list_for_each(ptr, &rdev->fence_drv.emited) {
				/* count up to 3, that's enought info */
				if (++not_processed >= 3)
					break;
			}
		}
		read_unlock_irqrestore(&rdev->fence_drv.lock, irq_flags);

		if (not_processed >= 3) { /* should upclock */
			if (rdev->pm.dynpm_planned_action == DYNPM_ACTION_DOWNCLOCK) {
				rdev->pm.dynpm_planned_action = DYNPM_ACTION_NONE;
			} else if (rdev->pm.dynpm_planned_action == DYNPM_ACTION_NONE &&
				   rdev->pm.dynpm_can_upclock) {
				rdev->pm.dynpm_planned_action =
					DYNPM_ACTION_UPCLOCK;
				rdev->pm.dynpm_action_timeout = jiffies +
				msecs_to_jiffies(RADEON_RECLOCK_DELAY_MS);
			}
		} else if (not_processed == 0) { /* should downclock */
			if (rdev->pm.dynpm_planned_action == DYNPM_ACTION_UPCLOCK) {
				rdev->pm.dynpm_planned_action = DYNPM_ACTION_NONE;
			} else if (rdev->pm.dynpm_planned_action == DYNPM_ACTION_NONE &&
				   rdev->pm.dynpm_can_downclock) {
				rdev->pm.dynpm_planned_action =
					DYNPM_ACTION_DOWNCLOCK;
				rdev->pm.dynpm_action_timeout = jiffies +
				msecs_to_jiffies(RADEON_RECLOCK_DELAY_MS);
			}
		}

		/* Note, radeon_pm_set_clocks is called with static_switch set
		 * to false since we want to wait for vbl to avoid flicker.
		 */
		if (rdev->pm.dynpm_planned_action != DYNPM_ACTION_NONE &&
		    jiffies > rdev->pm.dynpm_action_timeout) {
			radeon_pm_get_dynpm_state(rdev);
			radeon_pm_set_clocks(rdev);
		}
	}
	mutex_unlock(&rdev->pm.mutex);
	ttm_bo_unlock_delayed_workqueue(&rdev->mman.bdev, resched);

	queue_delayed_work(rdev->wq, &rdev->pm.dynpm_idle_work,
					msecs_to_jiffies(RADEON_IDLE_LOOP_MS));
}

/*
 * Debugfs info
 */
#if defined(CONFIG_DEBUG_FS)

static int radeon_debugfs_pm_info(struct seq_file *m, void *data)
{
	struct drm_info_node *node = (struct drm_info_node *) m->private;
	struct drm_device *dev = node->minor->dev;
	struct radeon_device *rdev = dev->dev_private;

	seq_printf(m, "default engine clock: %u0 kHz\n", rdev->clock.default_sclk);
	seq_printf(m, "current engine clock: %u0 kHz\n", radeon_get_engine_clock(rdev));
	seq_printf(m, "default memory clock: %u0 kHz\n", rdev->clock.default_mclk);
	if (rdev->asic->get_memory_clock)
		seq_printf(m, "current memory clock: %u0 kHz\n", radeon_get_memory_clock(rdev));
	if (rdev->asic->get_pcie_lanes)
		seq_printf(m, "PCIE lanes: %d\n", radeon_get_pcie_lanes(rdev));

	return 0;
}

static struct drm_info_list radeon_pm_info_list[] = {
	{"radeon_pm_info", radeon_debugfs_pm_info, 0, NULL},
};
#endif

static int radeon_debugfs_pm_init(struct radeon_device *rdev)
{
#if defined(CONFIG_DEBUG_FS)
	return radeon_debugfs_add_files(rdev, radeon_pm_info_list, ARRAY_SIZE(radeon_pm_info_list));
#else
	return 0;
#endif
}

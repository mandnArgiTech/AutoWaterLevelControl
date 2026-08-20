package com.flm.platform.api.service;

import com.flm.platform.api.dto.DeviceModelSummary;
import com.flm.platform.domain.DeviceModel;
import com.flm.platform.domain.DeviceModelRepository;
import org.springframework.stereotype.Service;
import java.util.List;

@Service
public class DeviceModelService {

    private final DeviceModelRepository deviceModelRepository;

    public DeviceModelService(DeviceModelRepository deviceModelRepository) {
        this.deviceModelRepository = deviceModelRepository;
    }

    public List<DeviceModelSummary> list() {
        return deviceModelRepository.findAll().stream().map(this::toSummary).toList();
    }

    public DeviceModelSummary getByKey(String modelKey) {
        DeviceModel m = deviceModelRepository.findByModelKey(modelKey)
            .orElseThrow(() -> new IllegalArgumentException("Device model not found: " + modelKey));
        return toSummary(m);
    }

    private DeviceModelSummary toSummary(DeviceModel m) {
        return new DeviceModelSummary(
            m.getId(), m.getModelKey(), m.getName(), m.getRole(),
            m.getBoard(), m.getMcu(), m.getDefaultComm(),
            m.getCapabilityProfile(), m.getProtectionProfile(), m.getInterlockTier(),
            m.getCreatedAt()
        );
    }
}

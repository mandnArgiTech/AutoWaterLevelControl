package com.flm.platform.domain;

import org.springframework.data.jpa.repository.JpaRepository;
import java.util.List;
import java.util.Optional;
import java.util.UUID;

public interface BindingRepository extends JpaRepository<Binding, UUID> {
    List<Binding> findByAssetIdOrderByValidFromDesc(UUID assetId);
    Optional<Binding> findByNodeCapabilityIdAndRoleAndValidToIsNull(UUID nodeCapabilityId, String role);
}

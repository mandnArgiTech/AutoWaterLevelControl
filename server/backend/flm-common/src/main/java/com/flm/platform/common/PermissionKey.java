package com.flm.platform.common;

/** Fine-grained permission keys for RBAC. */
public final class PermissionKey {
    private PermissionKey() {}

    public static final String PLATFORM_MQTT_READ = "PLATFORM_MQTT_READ";
    public static final String PLATFORM_MQTT_WRITE = "PLATFORM_MQTT_WRITE";
    public static final String PLATFORM_DB_READ = "PLATFORM_DB_READ";
    public static final String PLATFORM_DB_BACKUP = "PLATFORM_DB_BACKUP";
    public static final String PLATFORM_DB_RESTORE = "PLATFORM_DB_RESTORE";
    public static final String PLATFORM_SQL_READ = "PLATFORM_SQL_READ";
    public static final String PLATFORM_SQL_WRITE = "PLATFORM_SQL_WRITE";
    public static final String USER_MANAGE = "USER_MANAGE";
    public static final String ROLE_MANAGE = "ROLE_MANAGE";
    public static final String VENDOR_MANAGE = "VENDOR_MANAGE";
    public static final String AUDIT_READ = "AUDIT_READ";
    public static final String DEVICE_MANAGE = "DEVICE_MANAGE";
}

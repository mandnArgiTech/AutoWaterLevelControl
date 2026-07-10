package com.flm.platform.admin.service;

import org.junit.jupiter.api.Test;
import static org.junit.jupiter.api.Assertions.*;

class SqlWorkspaceServiceTest {

    @Test
    void classifyReadSelect() {
        SqlWorkspaceService svc = new SqlWorkspaceService(null, null, null, 500, 30);
        assertEquals(SqlWorkspaceService.SqlClass.READ, svc.classify("SELECT * FROM users"));
    }

    @Test
    void classifyDestructiveDrop() {
        SqlWorkspaceService svc = new SqlWorkspaceService(null, null, null, 500, 30);
        assertEquals(SqlWorkspaceService.SqlClass.DESTRUCTIVE, svc.classify("DROP TABLE users"));
    }

    @Test
    void classifyWriteUpdate() {
        SqlWorkspaceService svc = new SqlWorkspaceService(null, null, null, 500, 30);
        assertEquals(SqlWorkspaceService.SqlClass.WRITE, svc.classify("UPDATE users SET active = true WHERE id = 'x'"));
    }
}

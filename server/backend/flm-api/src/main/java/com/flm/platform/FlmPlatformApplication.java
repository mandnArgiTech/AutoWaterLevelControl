package com.flm.platform;

import org.springframework.boot.SpringApplication;
import org.springframework.boot.autoconfigure.SpringBootApplication;
import org.springframework.boot.autoconfigure.domain.EntityScan;
import org.springframework.data.jpa.repository.config.EnableJpaRepositories;
import org.springframework.scheduling.annotation.EnableScheduling;

@SpringBootApplication(scanBasePackages = "com.flm.platform")
@EntityScan("com.flm.platform.domain")
@EnableJpaRepositories("com.flm.platform.domain")
@EnableScheduling
public class FlmPlatformApplication {
    public static void main(String[] args) {
        SpringApplication.run(FlmPlatformApplication.class, args);
    }
}

# Build stage
FROM maven:3.9-eclipse-temurin-21 AS build
WORKDIR /app
COPY pom.xml .
COPY flm-common/pom.xml flm-common/
COPY flm-domain/pom.xml flm-domain/
COPY flm-security/pom.xml flm-security/
COPY flm-mqtt-bridge/pom.xml flm-mqtt-bridge/
COPY flm-api/pom.xml flm-api/
RUN mvn -q -pl flm-api -am dependency:go-offline
COPY . .
RUN mvn -q -pl flm-api -am package -DskipTests

# Runtime
FROM eclipse-temurin:21-jre-alpine
WORKDIR /app
COPY --from=build /app/flm-api/target/flm-api-*.jar app.jar
EXPOSE 8080
ENTRYPOINT ["java", "-jar", "app.jar"]

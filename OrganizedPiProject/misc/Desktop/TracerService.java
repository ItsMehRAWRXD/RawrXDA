// ai-cli-backend/src/main/java/com/aicli/monitoring/TracerService.java
package com.aicli.monitoring;

import io.opentelemetry.api.trace.Span;
import io.opentelemetry.api.trace.Tracer;
import io.opentelemetry.api.trace.SpanBuilder;
import io.opentelemetry.api.trace.SpanKind;
import io.opentelemetry.context.Scope;
import io.opentelemetry.sdk.OpenTelemetrySdk;
import io.opentelemetry.sdk.trace.SdkTracerProvider;
import io.opentelemetry.sdk.trace.export.SimpleSpanProcessor;
import io.opentelemetry.exporter.logging.LoggingSpanExporter;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;
import java.util.logging.Logger;
import java.util.logging.Level;

/**
 * Distributed tracing service using OpenTelemetry.
 */
public class TracerService {
    private static final Logger logger = Logger.getLogger(TracerService.class.getName());
    
    private final Tracer tracer;
    private final Map<String, Span> activeSpans = new ConcurrentHashMap<>();
    private final OpenTelemetrySdk openTelemetry;
    
    public TracerService() {
        this.openTelemetry = initializeOpenTelemetry();
        this.tracer = openTelemetry.getTracer("ai-cli-tracer");
        logger.info("TracerService initialized");
    }
    
    /**
     * Initialize OpenTelemetry SDK
     */
    private OpenTelemetrySdk initializeOpenTelemetry() {
        SdkTracerProvider tracerProvider = SdkTracerProvider.builder()
                .addSpanProcessor(SimpleSpanProcessor.create(LoggingSpanExporter.create()))
                .build();
        
        return OpenTelemetrySdk.builder()
                .setTracerProvider(tracerProvider)
                .buildAndRegisterGlobal();
    }
    
    /**
     * Create a new span for plugin execution
     */
    public Span createPluginExecutionSpan(String pluginId, String operation) {
        Span span = tracer.spanBuilder("plugin." + operation)
                .setSpanKind(SpanKind.INTERNAL)
                .setAttribute("plugin.id", pluginId)
                .setAttribute("plugin.operation", operation)
                .startSpan();
        
        activeSpans.put(pluginId + "." + operation, span);
        logger.fine("Created plugin execution span: " + pluginId + "." + operation);
        return span;
    }
    
    /**
     * Create a new span for API requests
     */
    public Span createApiSpan(String endpoint, String method, String userId) {
        Span span = tracer.spanBuilder("api." + endpoint)
                .setSpanKind(SpanKind.SERVER)
                .setAttribute("http.method", method)
                .setAttribute("http.route", endpoint)
                .setAttribute("user.id", userId)
                .startSpan();
        
        String spanKey = "api." + endpoint + "." + System.currentTimeMillis();
        activeSpans.put(spanKey, span);
        return span;
    }
    
    /**
     * Complete and close a span
     */
    public void completeSpan(Span span) {
        if (span != null) {
            span.end();
            activeSpans.entrySet().removeIf(entry -> entry.getValue().equals(span));
            logger.fine("Completed span: " + span.getName());
        }
    }
    
    /**
     * Complete span with error status
     */
    public void completeSpanWithError(Span span, Throwable error) {
        if (span != null) {
            span.setStatus(io.opentelemetry.api.trace.StatusCode.ERROR, error.getMessage());
            span.recordException(error);
            span.end();
            activeSpans.entrySet().removeIf(entry -> entry.getValue().equals(span));
            logger.fine("Completed span with error: " + span.getName());
        }
    }
    
    /**
     * Get tracer instance
     */
    public Tracer getTracer() {
        return tracer;
    }
    
    /**
     * Get number of active spans
     */
    public int getActiveSpanCount() {
        return activeSpans.size();
    }
    
    /**
     * Cleanup all active spans
     */
    public void cleanup() {
        activeSpans.values().forEach(this::completeSpan);
        activeSpans.clear();
        try {
            openTelemetry.close();
        } catch (Exception e) {
            logger.log(Level.WARNING, "Error closing OpenTelemetry", e);
        }
        logger.info("TracerService cleaned up");
    }
}

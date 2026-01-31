// n0mn0m Meta-Engine Easter Egg Parser
// Detects and executes hidden magic in comments

package meta

import (
	"encoding/hex"
	"fmt"
	"log"
	"strings"
	"time"
)

// EasterEgg represents a hidden executable comment
type EasterEgg struct {
	HexCode    string
	Message    string
	Action     string
	Duration   time.Duration
}

// Known Easter Eggs
var easterEggs = map[string]EasterEgg{
	"0x4E4F434F4E534F4E53554D4544": {
		HexCode:  "0x4E4F434F4E534F4E53554D4544",
		Message:  "NO CONS, ONLY CHOICES — SUMMONED.",
		Action:   "confetti_burst",
		Duration: 60 * time.Second,
	},
	"0x5452494C4C494F4E444F4C4C4152": {
		HexCode:  "0x5452494C4C494F4E444F4C4C4152",
		Message:  "TRILLION DOLLAR — ACTIVATED.",
		Action:   "golden_rain",
		Duration: 120 * time.Second,
	},
	"0x4D455441454E47494E454D41474943": {
		HexCode:  "0x4D455441454E47494E454D41474943",
		Message:  "META ENGINE MAGIC — UNLEASHED.",
		Action:   "rainbow_wave",
		Duration: 90 * time.Second,
	},
}

// ParseEasterEgg extracts and executes hidden magic from source code
func ParseEasterEgg(sourceCode string) (*EasterEgg, error) {
	lines := strings.Split(sourceCode, "\n")
	
	for _, line := range lines {
		// Look for comments with hex codes
		if strings.Contains(line, "//") {
			comment := strings.TrimSpace(strings.Split(line, "//")[1])
			
			// Check if it's a hex code
			if strings.HasPrefix(comment, "0x") && len(comment) > 2 {
				hexCode := strings.ToUpper(comment)
				
				// Check if it's a known Easter egg
				if egg, exists := easterEggs[hexCode]; exists {
					// Decode the hex to verify it's valid
					decoded, err := hex.DecodeString(hexCode[2:])
					if err != nil {
						continue
					}
					
					// Log the magic message
					log.Printf("🎉 EASTER EGG DETECTED: %s", egg.Message)
					
					// Execute the action
					executeEasterEggAction(egg)
					
					return &egg, nil
				}
			}
		}
	}
	
	return nil, nil
}

// executeEasterEggAction performs the magic
func executeEasterEggAction(egg EasterEgg) {
	switch egg.Action {
	case "confetti_burst":
		triggerConfettiBurst()
		zeroRateUsage(egg.Duration)
	case "golden_rain":
		triggerGoldenRain()
		zeroRateUsage(egg.Duration)
	case "rainbow_wave":
		triggerRainbowWave()
		zeroRateUsage(egg.Duration)
	}
}

// triggerConfettiBurst sends confetti animation to CerebralIDE
func triggerConfettiBurst() {
	// Send gRPC message to CerebralIDE
	event := &UIEvent{
		Type: "confetti_burst",
		Data: map[string]interface{}{
			"duration": 5000, // 5 seconds
			"colors":   []string{"#ff6b6b", "#4ecdc4", "#45b7d1", "#96ceb4", "#feca57"},
			"message":  "NO CONS, ONLY CHOICES — SUMMONED.",
		},
	}
	
	sendUIEvent(event)
}

// triggerGoldenRain sends golden rain animation
func triggerGoldenRain() {
	event := &UIEvent{
		Type: "golden_rain",
		Data: map[string]interface{}{
			"duration": 8000,
			"message":  "TRILLION DOLLAR — ACTIVATED.",
		},
	}
	
	sendUIEvent(event)
}

// triggerRainbowWave sends rainbow wave animation
func triggerRainbowWave() {
	event := &UIEvent{
		Type: "rainbow_wave",
		Data: map[string]interface{}{
			"duration": 6000,
			"message":  "META ENGINE MAGIC — UNLEASHED.",
		},
	}
	
	sendUIEvent(event)
}

// zeroRateUsage temporarily disables billing for demo purposes
func zeroRateUsage(duration time.Duration) {
	log.Printf("💰 Billing zero-rated for %v (demo mode)", duration)
	
	// Set a flag in the billing system
	setBillingOverride(true, duration)
	
	// Schedule restoration of normal billing
	time.AfterFunc(duration, func() {
		setBillingOverride(false, 0)
		log.Printf("💰 Normal billing restored")
	})
}

// UIEvent represents an event sent to the CerebralIDE
type UIEvent struct {
	Type string
	Data map[string]interface{}
}

// sendUIEvent sends events to the CerebralIDE via gRPC
func sendUIEvent(event *UIEvent) {
	// This would connect to the CerebralIDE's gRPC service
	// and send the animation event
	log.Printf("🎨 Sending UI event: %s", event.Type)
}

// setBillingOverride controls the billing system
func setBillingOverride(override bool, duration time.Duration) {
	// This would interface with the Stripe billing system
	// to temporarily disable charges
	log.Printf("💳 Billing override: %v for %v", override, duration)
}

// DecodeHexMessage converts hex to ASCII for debugging
func DecodeHexMessage(hexCode string) string {
	// Remove 0x prefix
	hexStr := strings.TrimPrefix(hexCode, "0x")
	
	// Decode hex to bytes
	decoded, err := hex.DecodeString(hexStr)
	if err != nil {
		return "Invalid hex"
	}
	
	// Convert to ASCII string
	return string(decoded)
}

// GetEasterEggList returns all available Easter eggs
func GetEasterEggList() map[string]string {
	eggs := make(map[string]string)
	
	for hexCode, egg := range easterEggs {
		decoded := DecodeHexMessage(hexCode)
		eggs[hexCode] = fmt.Sprintf("%s -> %s", decoded, egg.Message)
	}
	
	return eggs
}

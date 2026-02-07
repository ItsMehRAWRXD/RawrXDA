// PotentialKeysList.java - Comprehensive list of API keys that Keyscan can discover
// This demonstrates the types of keys the tool can find across different providers

import java.util.*;

public class PotentialKeysList {
    
    public static void main(String[] args) {
        System.out.println("? POTENTIAL API KEYS DISCOVERABLE BY KEYSCAN");
        System.out.println("=" + "=".repeat(60));
        
        displayAllKeyTypes();
    }
    
    public static void displayAllKeyTypes() {
        System.out.println("\nAI/ML PROVIDERS:");
        displayAIProviders();
        
        System.out.println("\nCLOUD PROVIDERS:");
        displayCloudProviders();
        
        System.out.println("\nPAYMENT PROCESSORS:");
        displayPaymentProviders();
        
        System.out.println("\nCOMMUNICATION SERVICES:");
        displayCommunicationServices();
        
        System.out.println("\nDEVELOPMENT TOOLS:");
        displayDevelopmentTools();
        
        System.out.println("\nSOCIAL/MESSAGING:");
        displaySocialServices();
        
        System.out.println("\nSECURITY SERVICES:");
        displaySecurityServices();
        
        System.out.println("\nANALYTICS & MONITORING:");
        displayAnalyticsServices();
        
        System.out.println("\nWEB SERVICES:");
        displayWebServices();
    }
    
    private static void displayAIProviders() {
        System.out.println("OpenAI Keys:");
        System.out.println("  • sk-abcdef1234567890abcdef1234567890abcdef12");
        System.out.println("  • org-abcdef1234567890abcdef1234567890");
        System.out.println("  • sk-proj-abcdef1234567890abcdef1234567890");
        
        System.out.println("\nGoogle AI Keys:");
        System.out.println("  • AIzaSyDdI0hCZtE6vySjMm-WEfRq3CPzqKqqsHI");
        System.out.println("  • AIzaSyBxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
        
        System.out.println("\nAnthropic Keys:");
        System.out.println("  • sk-ant-api03-abcdef1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef1234567890");
        
        System.out.println("\nHugging Face Keys:");
        System.out.println("  • hf_abcdef1234567890abcdef1234567890");
        
        System.out.println("\nCohere Keys:");
        System.out.println("  • abcdef1234567890abcdef1234567890abcdef1234");
        
        System.out.println("\nMistral Keys:");
        System.out.println("  • abcdef1234567890abcdef1234567890");
    }
    
    private static void displayCloudProviders() {
        System.out.println("AWS Keys:");
        System.out.println("  • AKIAIOSFODNN7EXAMPLE");
        System.out.println("  • wJalrXUtnFEMI/K7MDENG/bPxRfiCYEXAMPLEKEY");
        
        System.out.println("\nGoogle Cloud Keys:");
        System.out.println("  • AIzaSyDdI0hCZtE6vySjMm-WEfRq3CPzqKqqsHI");
        System.out.println("  • 1//0abcdef1234567890abcdef1234567890abcdef1234567890");
        
        System.out.println("\nAzure Keys:");
        System.out.println("  • abcdef1234567890abcdef1234567890abcdef1234567890");
        System.out.println("  • https://vault.azure.net/secrets/secret-name/version");
        
        System.out.println("\nDigitalOcean Keys:");
        System.out.println("  • abcdef1234567890abcdef1234567890abcdef1234567890");
        
        System.out.println("\nLinode Keys:");
        System.out.println("  • abcdef1234567890abcdef1234567890abcdef1234567890");
    }
    
    private static void displayPaymentProviders() {
        System.out.println("Stripe Keys:");
        System.out.println("  • sk_live_abcdef1234567890abcdef1234567890");
        System.out.println("  • sk_test_abcdef1234567890abcdef1234567890");
        System.out.println("  • pk_live_abcdef1234567890abcdef1234567890");
        System.out.println("  • pk_test_abcdef1234567890abcdef1234567890");
        
        System.out.println("\nPayPal Keys:");
        System.out.println("  • abcdef1234567890abcdef1234567890abcdef1234567890");
        
        System.out.println("\nSquare Keys:");
        System.out.println("  • abcdef1234567890abcdef1234567890abcdef1234567890");
        
        System.out.println("\nRazorpay Keys:");
        System.out.println("  • rzp_test_abcdef1234567890abcdef1234567890");
        System.out.println("  • rzp_live_abcdef1234567890abcdef1234567890");
    }
    
    private static void displayCommunicationServices() {
        System.out.println("SendGrid Keys:");
        System.out.println("  • SG.abcdef1234567890abcdef1234567890.abcdef1234567890abcdef1234567890abcdef1234567890");
        
        System.out.println("\nMailgun Keys:");
        System.out.println("  • key-abcdef1234567890abcdef1234567890");
        
        System.out.println("\nTwilio Keys:");
        System.out.println("  • ACabcdef1234567890abcdef1234567890");
        System.out.println("  • abcdef1234567890abcdef1234567890");
        
        System.out.println("\nAWS SES Keys:");
        System.out.println("  • AKIAIOSFODNN7EXAMPLE");
        System.out.println("  • wJalrXUtnFEMI/K7MDENG/bPxRfiCYEXAMPLEKEY");
        
        System.out.println("\nPostmark Keys:");
        System.out.println("  • abcdef1234567890abcdef1234567890abcdef1234567890");
    }
    
    private static void displayDevelopmentTools() {
        System.out.println("GitHub Keys:");
        System.out.println("  • ghp_abcdef1234567890abcdef1234567890abcdef");
        System.out.println("  • gho_abcdef1234567890abcdef1234567890abcdef");
        System.out.println("  • github_pat_abcdef1234567890abcdef1234567890abcdef1234567890abcdef1234567890");
        
        System.out.println("\nGitLab Keys:");
        System.out.println("  • glpat-abcdef1234567890abcdef1234567890abcdef1234567890");
        
        System.out.println("\nBitbucket Keys:");
        System.out.println("  • abcdef1234567890abcdef1234567890abcdef1234567890");
        
        System.out.println("\nDocker Hub Keys:");
        System.out.println("  • dckr_pat_abcdef1234567890abcdef1234567890abcdef1234567890");
        
        System.out.println("\nNPM Keys:");
        System.out.println("  • npm_abcdef1234567890abcdef1234567890abcdef1234567890");
    }
    
    private static void displaySocialServices() {
        System.out.println("Slack Keys:");
        System.out.println("  • xoxb-abcdef1234567890abcdef1234567890abcdef1234567890");
        System.out.println("  • xoxp-abcdef1234567890abcdef1234567890abcdef1234567890");
        
        System.out.println("\nDiscord Keys:");
        System.out.println("  • abcdef1234567890abcdef1234567890.abcdef1234567890.abcdef1234567890");
        
        System.out.println("\nTelegram Keys:");
        System.out.println("  • 1234567890:abcdef1234567890abcdef1234567890abcdef1234567890");
        
        System.out.println("\nWhatsApp Keys:");
        System.out.println("  • abcdef1234567890abcdef1234567890abcdef1234567890");
        
        System.out.println("\nFacebook Keys:");
        System.out.println("  • abcdef1234567890abcdef1234567890abcdef1234567890");
    }
    
    private static void displaySecurityServices() {
        System.out.println("Auth0 Keys:");
        System.out.println("  • abcdef1234567890abcdef1234567890abcdef1234567890");
        
        System.out.println("\nFirebase Keys:");
        System.out.println("  • AIzaSyDdI0hCZtE6vySjMm-WEfRq3CPzqKqqsHI");
        
        System.out.println("\nJWT Keys:");
        System.out.println("  • abcdef1234567890abcdef1234567890abcdef1234567890");
        
        System.out.println("\nOAuth Keys:");
        System.out.println("  • abcdef1234567890abcdef1234567890abcdef1234567890");
    }
    
    private static void displayAnalyticsServices() {
        System.out.println("Google Analytics Keys:");
        System.out.println("  • G-abcdef1234567890abcdef1234567890");
        System.out.println("  • UA-abcdef1234567890-1");
        
        System.out.println("\nMixpanel Keys:");
        System.out.println("  • abcdef1234567890abcdef1234567890abcdef1234567890");
        
        System.out.println("\nAmplitude Keys:");
        System.out.println("  • abcdef1234567890abcdef1234567890abcdef1234567890");
        
        System.out.println("\nSegment Keys:");
        System.out.println("  • abcdef1234567890abcdef1234567890abcdef1234567890");
    }
    
    private static void displayWebServices() {
        System.out.println("CDN Keys:");
        System.out.println("  • Cloudflare: abcdef1234567890abcdef1234567890abcdef1234567890");
        System.out.println("  • AWS CloudFront: abcdef1234567890abcdef1234567890abcdef1234567890");
        
        System.out.println("\nDatabase Keys:");
        System.out.println("  • MongoDB: abcdef1234567890abcdef1234567890abcdef1234567890");
        System.out.println("  • Redis: abcdef1234567890abcdef1234567890abcdef1234567890");
        
        System.out.println("\nStorage Keys:");
        System.out.println("  • AWS S3: abcdef1234567890abcdef1234567890abcdef1234567890");
        System.out.println("  • Google Cloud Storage: abcdef1234567890abcdef1234567890abcdef1234567890");
    }
    
    // Common patterns for key detection
    public static void displayCommonPatterns() {
        System.out.println("\nCOMMON KEY PATTERNS:");
        System.out.println("=" + "=".repeat(40));
        
        System.out.println("OpenAI Pattern: sk-[a-zA-Z0-9]{48}");
        System.out.println("Google AI Pattern: AIza[0-9A-Za-z\\-_]{35}");
        System.out.println("AWS Access Key: AKIA[0-9A-Z]{16}");
        System.out.println("GitHub Token: ghp_[a-zA-Z0-9]{36}");
        System.out.println("Stripe Live Key: sk_live_[a-zA-Z0-9]{24}");
        System.out.println("SendGrid Key: SG\\.[a-zA-Z0-9\\-_]{22}\\.[a-zA-Z0-9\\-_]{43}");
        System.out.println("Twilio SID: AC[a-zA-Z0-9]{32}");
        System.out.println("Slack Bot Token: xoxb-[a-zA-Z0-9\\-_]{48}");
    }
    
    // Environment variable names commonly found
    public static void displayCommonEnvVars() {
        System.out.println("\nCOMMON ENVIRONMENT VARIABLES:");
        System.out.println("=" + "=".repeat(40));
        
        System.out.println("OPENAI_API_KEY=sk-...");
        System.out.println("GEMINI_API_KEY=AIza...");
        System.out.println("ANTHROPIC_API_KEY=sk-ant-...");
        System.out.println("AWS_ACCESS_KEY_ID=AKIA...");
        System.out.println("AWS_SECRET_ACCESS_KEY=...");
        System.out.println("STRIPE_SECRET_KEY=sk_live_...");
        System.out.println("GITHUB_TOKEN=ghp_...");
        System.out.println("SENDGRID_API_KEY=SG...");
        System.out.println("TWILIO_ACCOUNT_SID=AC...");
        System.out.println("SLACK_BOT_TOKEN=xoxb-...");
    }
}

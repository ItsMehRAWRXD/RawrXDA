#!/usr/bin/env python3
"""
Privacy & Data Leakage Prevention Auditor
Scans agent communications, logs, and data flows for sensitive information exposure
Implements GDPR/CCPA compliance checks and data retention policies
"""

import re
import json
import hashlib
from pathlib import Path
from typing import Dict, List, Set, Tuple, Optional
from dataclasses import dataclass, field
from datetime import datetime, timedelta
import logging

logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)


@dataclass
class PIIPattern:
    """Pattern definition for detecting Personally Identifiable Information"""
    name: str
    pattern: str
    category: str  # 'email', 'ssn', 'credit_card', 'phone', 'ip', 'api_key', etc.
    severity: str  # 'critical', 'high', 'medium', 'low'
    regex: re.Pattern = field(init=False)
    
    def __post_init__(self):
        self.regex = re.compile(self.pattern, re.IGNORECASE)


@dataclass
class PIIDetection:
    """Detected PII instance"""
    file_path: str
    line_number: int
    category: str
    severity: str
    matched_text: str
    redacted_text: str
    context: str


@dataclass
class DataRetentionPolicy:
    """Data retention policy definition"""
    data_type: str
    retention_days: int
    deletion_required: bool
    anonymization_allowed: bool


class PrivacyAuditor:
    """Main privacy auditing and compliance checking engine"""
    
    def __init__(self):
        self.pii_patterns = self._initialize_pii_patterns()
        self.retention_policies = self._initialize_retention_policies()
        self.detections: List[PIIDetection] = []
        self.masked_data: Dict[str, str] = {}
        
    def _initialize_pii_patterns(self) -> List[PIIPattern]:
        """Initialize PII detection patterns"""
        return [
            # Email addresses
            PIIPattern(
                name="Email Address",
                pattern=r'\b[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Z|a-z]{2,}\b',
                category='email',
                severity='high'
            ),
            # US Social Security Numbers
            PIIPattern(
                name="Social Security Number",
                pattern=r'\b\d{3}-\d{2}-\d{4}\b',
                category='ssn',
                severity='critical'
            ),
            # Credit Card Numbers (simple pattern)
            PIIPattern(
                name="Credit Card Number",
                pattern=r'\b(?:\d{4}[-\s]?){3}\d{4}\b',
                category='credit_card',
                severity='critical'
            ),
            # Phone Numbers (US format)
            PIIPattern(
                name="Phone Number",
                pattern=r'\b(?:\+?1[-.\s]?)?\(?\d{3}\)?[-.\s]?\d{3}[-.\s]?\d{4}\b',
                category='phone',
                severity='medium'
            ),
            # IPv4 Addresses
            PIIPattern(
                name="IPv4 Address",
                pattern=r'\b(?:\d{1,3}\.){3}\d{1,3}\b',
                category='ip_address',
                severity='low'
            ),
            # API Keys (generic patterns)
            PIIPattern(
                name="API Key",
                pattern=r'\b[A-Za-z0-9]{32,}\b',
                category='api_key',
                severity='critical'
            ),
            # AWS Access Keys
            PIIPattern(
                name="AWS Access Key",
                pattern=r'\bAKIA[0-9A-Z]{16}\b',
                category='aws_key',
                severity='critical'
            ),
            # GitHub Tokens
            PIIPattern(
                name="GitHub Token",
                pattern=r'\bghp_[A-Za-z0-9]{36}\b',
                category='github_token',
                severity='critical'
            ),
            # Bearer Tokens
            PIIPattern(
                name="Bearer Token",
                pattern=r'\bBearer\s+[A-Za-z0-9\-._~+/]+=*',
                category='bearer_token',
                severity='critical'
            ),
            # JWT Tokens
            PIIPattern(
                name="JWT Token",
                pattern=r'\beyJ[A-Za-z0-9_-]*\.eyJ[A-Za-z0-9_-]*\.[A-Za-z0-9_-]*\b',
                category='jwt',
                severity='critical'
            ),
        ]
    
    def _initialize_retention_policies(self) -> Dict[str, DataRetentionPolicy]:
        """Initialize data retention policies (GDPR/CCPA compliant)"""
        return {
            'logs': DataRetentionPolicy('logs', 90, True, False),
            'telemetry': DataRetentionPolicy('telemetry', 30, True, True),
            'user_data': DataRetentionPolicy('user_data', 365, True, True),
            'session_data': DataRetentionPolicy('session_data', 7, True, False),
            'error_reports': DataRetentionPolicy('error_reports', 180, True, True),
            'analytics': DataRetentionPolicy('analytics', 180, True, True),
        }
    
    def scan_file(self, file_path: Path) -> List[PIIDetection]:
        """Scan a single file for PII exposure"""
        detections = []
        
        try:
            with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
                for line_num, line in enumerate(f, 1):
                    for pattern in self.pii_patterns:
                        matches = pattern.regex.finditer(line)
                        for match in matches:
                            matched_text = match.group()
                            redacted = self._redact_pii(matched_text, pattern.category)
                            
                            detection = PIIDetection(
                                file_path=str(file_path),
                                line_number=line_num,
                                category=pattern.category,
                                severity=pattern.severity,
                                matched_text=matched_text,
                                redacted_text=redacted,
                                context=line.strip()
                            )
                            detections.append(detection)
                            self.detections.append(detection)
                            
                            logger.warning(
                                f"PII detected in {file_path}:{line_num} - "
                                f"{pattern.name} ({pattern.severity})"
                            )
        except Exception as e:
            logger.error(f"Error scanning {file_path}: {e}")
        
        return detections
    
    def scan_directory(self, directory: Path, patterns: List[str] = None) -> List[PIIDetection]:
        """Scan directory for PII in matching files"""
        if patterns is None:
            patterns = ['*.py', '*.log', '*.txt', '*.json', '*.md']
        
        all_detections = []
        for pattern in patterns:
            for file_path in directory.rglob(pattern):
                if file_path.is_file():
                    detections = self.scan_file(file_path)
                    all_detections.extend(detections)
        
        return all_detections
    
    def _redact_pii(self, text: str, category: str) -> str:
        """Redact PII based on category"""
        if category == 'email':
            # Keep domain for debugging, mask username
            parts = text.split('@')
            if len(parts) == 2:
                return f"{parts[0][:2]}***@{parts[1]}"
        elif category in ['ssn', 'credit_card']:
            # Show last 4 digits only
            return f"***-**-{text[-4:]}" if len(text) >= 4 else "***"
        elif category == 'phone':
            # Mask all but last 4 digits
            digits = re.sub(r'\D', '', text)
            return f"***-***-{digits[-4:]}" if len(digits) >= 4 else "***"
        elif category in ['api_key', 'aws_key', 'github_token', 'bearer_token', 'jwt']:
            # Show first 4 chars only
            return f"{text[:4]}...{text[-4:]}" if len(text) > 8 else "***"
        elif category == 'ip_address':
            # Mask last octet
            parts = text.split('.')
            if len(parts) == 4:
                return f"{'.'.join(parts[:3])}.***"
        
        return "***REDACTED***"
    
    def mask_data(self, data: str) -> str:
        """Apply masking to all detected PII patterns in data"""
        masked = data
        for pattern in self.pii_patterns:
            masked = pattern.regex.sub(
                lambda m: self._redact_pii(m.group(), pattern.category),
                masked
            )
        return masked
    
    def check_retention_compliance(self, file_path: Path, data_type: str) -> bool:
        """Check if file complies with data retention policy"""
        if data_type not in self.retention_policies:
            logger.warning(f"No retention policy defined for {data_type}")
            return True
        
        policy = self.retention_policies[data_type]
        
        # Check file age
        file_age = datetime.now() - datetime.fromtimestamp(file_path.stat().st_mtime)
        retention_period = timedelta(days=policy.retention_days)
        
        if file_age > retention_period:
            if policy.deletion_required:
                logger.warning(
                    f"File {file_path} exceeds retention policy "
                    f"({file_age.days} days > {policy.retention_days} days)"
                )
                return False
        
        return True
    
    def anonymize_data(self, data: dict, fields_to_anonymize: List[str]) -> dict:
        """Anonymize specific fields in structured data"""
        anonymized = data.copy()
        
        for field in fields_to_anonymize:
            if field in anonymized:
                # Generate deterministic but irreversible hash
                value = str(anonymized[field])
                hashed = hashlib.sha256(value.encode()).hexdigest()[:16]
                anonymized[field] = f"anon_{hashed}"
        
        return anonymized
    
    def generate_compliance_report(self, output_path: Path):
        """Generate GDPR/CCPA compliance report"""
        report = {
            'scan_timestamp': datetime.now().isoformat(),
            'total_detections': len(self.detections),
            'by_severity': {},
            'by_category': {},
            'files_scanned': len(set(d.file_path for d in self.detections)),
            'critical_findings': [],
            'recommendations': []
        }
        
        # Group by severity
        for detection in self.detections:
            report['by_severity'][detection.severity] = \
                report['by_severity'].get(detection.severity, 0) + 1
            report['by_category'][detection.category] = \
                report['by_category'].get(detection.category, 0) + 1
            
            if detection.severity == 'critical':
                report['critical_findings'].append({
                    'file': detection.file_path,
                    'line': detection.line_number,
                    'category': detection.category,
                    'redacted': detection.redacted_text
                })
        
        # Generate recommendations
        if report['total_detections'] > 0:
            report['recommendations'].append(
                "Implement automatic PII redaction in logging systems"
            )
        if report['by_severity'].get('critical', 0) > 0:
            report['recommendations'].append(
                "URGENT: Remove or encrypt critical PII (API keys, credentials)"
            )
        if report['by_category'].get('email', 0) > 10:
            report['recommendations'].append(
                "Consider pseudonymization for email addresses in analytics"
            )
        
        # Write report
        with open(output_path, 'w') as f:
            json.dump(report, f, indent=2)
        
        logger.info(f"Compliance report written to {output_path}")
        return report
    
    def enforce_retention_policy(self, directory: Path, data_type: str, dry_run: bool = True):
        """Enforce data retention policy on directory"""
        if data_type not in self.retention_policies:
            logger.error(f"Unknown data type: {data_type}")
            return
        
        policy = self.retention_policies[data_type]
        retention_period = timedelta(days=policy.retention_days)
        deleted_count = 0
        
        for file_path in directory.rglob('*'):
            if not file_path.is_file():
                continue
            
            file_age = datetime.now() - datetime.fromtimestamp(file_path.stat().st_mtime)
            
            if file_age > retention_period:
                if dry_run:
                    logger.info(f"[DRY RUN] Would delete: {file_path} (age: {file_age.days} days)")
                else:
                    try:
                        file_path.unlink()
                        logger.info(f"Deleted: {file_path} (age: {file_age.days} days)")
                        deleted_count += 1
                    except Exception as e:
                        logger.error(f"Failed to delete {file_path}: {e}")
        
        logger.info(f"Retention policy enforcement complete. Files deleted: {deleted_count}")


def main():
    """CLI interface for privacy auditor"""
    import argparse
    
    parser = argparse.ArgumentParser(description='Privacy & Data Leakage Prevention Auditor')
    parser.add_argument('path', type=Path, help='File or directory to scan')
    parser.add_argument('--report', type=Path, default='privacy_audit_report.json',
                        help='Output report path')
    parser.add_argument('--enforce-retention', type=str,
                        help='Enforce retention policy for data type')
    parser.add_argument('--dry-run', action='store_true',
                        help='Dry run for retention enforcement')
    
    args = parser.parse_args()
    
    auditor = PrivacyAuditor()
    
    # Scan for PII
    if args.path.is_file():
        auditor.scan_file(args.path)
    else:
        auditor.scan_directory(args.path)
    
    # Generate report
    report = auditor.generate_compliance_report(args.report)
    
    print(f"\n=== Privacy Audit Summary ===")
    print(f"Total PII detections: {report['total_detections']}")
    print(f"Files scanned: {report['files_scanned']}")
    print(f"\nBy Severity:")
    for severity, count in sorted(report['by_severity'].items()):
        print(f"  {severity}: {count}")
    print(f"\nBy Category:")
    for category, count in sorted(report['by_category'].items()):
        print(f"  {category}: {count}")
    
    if report['critical_findings']:
        print(f"\n⚠️  CRITICAL: {len(report['critical_findings'])} critical findings require immediate attention")
    
    # Enforce retention policy if requested
    if args.enforce_retention:
        auditor.enforce_retention_policy(args.path, args.enforce_retention, args.dry_run)


if __name__ == '__main__':
    main()

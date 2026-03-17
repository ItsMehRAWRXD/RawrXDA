#!/usr/bin/env python3
"""
Privacy & Compliance Integration for NASM IDE
Combines PII detection, GDPR/CCPA compliance checking, and automated remediation
"""

import json
import sys
from pathlib import Path
from typing import Dict, List
from privacy_auditor import PrivacyAuditor
from gdpr_compliance_checker import GDPRComplianceChecker, CCPAComplianceChecker


class PrivacyComplianceIntegration:
    """Integrated privacy and compliance framework"""
    
    def __init__(self, workspace_root: Path):
        self.workspace_root = workspace_root
        self.privacy_auditor = PrivacyAuditor()
        self.gdpr_checker = GDPRComplianceChecker()
        self.ccpa_checker = CCPAComplianceChecker()
    
    def full_privacy_audit(self) -> Dict:
        """Run complete privacy audit on workspace"""
        print("[*] Starting full privacy audit...")
        
        # Scan for PII
        print("[*] Scanning for PII in codebase...")
        scan_results = self.privacy_auditor.scan_directory(
            self.workspace_root,
            patterns=['*.py', '*.asm', '*.log', '*.txt', '*.json', '*.md']
        )
        
        # Check retention compliance
        print("[*] Checking data retention compliance...")
        retention_violations = self.privacy_auditor.check_retention_compliance(
            self.workspace_root / 'logs'
        )
        
        # Generate compliance report
        report = {
            'pii_scan': {
                'files_scanned': len(scan_results),
                'pii_detections': sum(len(v) for v in scan_results.values()),
                'high_severity': sum(
                    1 for detections in scan_results.values()
                    for d in detections
                    if d.severity == 'high'
                )
            },
            'retention': {
                'violations': len(retention_violations)
            }
        }
        
        print(f"[+] Privacy audit complete: {report['pii_scan']['pii_detections']} PII instances found")
        return report
    
    def check_compliance(self, config_path: Path) -> Dict:
        """Check GDPR/CCPA compliance"""
        print("[*] Checking GDPR/CCPA compliance...")
        
        with open(config_path) as f:
            config = json.load(f)
        
        # GDPR checks
        self.gdpr_checker.check_data_minimization(config)
        self.gdpr_checker.check_storage_limitation(config.get('retention_days', 90))
        self.gdpr_checker.check_consent_mechanism(config)
        self.gdpr_checker.check_data_portability(config)
        self.gdpr_checker.check_right_to_erasure(config)
        self.gdpr_checker.check_encryption(config)
        
        # CCPA checks
        self.ccpa_checker.check_notice_at_collection(config)
        self.ccpa_checker.check_do_not_sell(config)
        
        gdpr_report = self.gdpr_checker.generate_report()
        ccpa_report = self.ccpa_checker.generate_report()
        
        print(f"[+] GDPR Compliance: {gdpr_report['overall_status']}")
        print(f"[+] CCPA Checks: {ccpa_report['summary']['compliant']}/{ccpa_report['summary']['total_checks']}")
        
        return {
            'gdpr': gdpr_report,
            'ccpa': ccpa_report
        }
    
    def auto_remediate(self, scan_results: Dict) -> int:
        """Automatically redact PII in files"""
        print("[*] Auto-remediating PII detections...")
        
        remediated_count = 0
        for file_path, detections in scan_results.items():
            if not detections:
                continue
            
            # Read file
            path = Path(file_path)
            with open(path) as f:
                content = f.read()
            
            # Redact all PII
            for detection in detections:
                redacted = self.privacy_auditor._redact_pii(detection.matched_text, detection.category)
                content = content.replace(detection.matched_text, redacted)
                remediated_count += 1
            
            # Write back
            with open(path, 'w') as f:
                f.write(content)
        
        print(f"[+] Remediated {remediated_count} PII instances")
        return remediated_count
    
    def generate_unified_report(self, output_path: Path):
        """Generate comprehensive privacy + compliance report"""
        print("[*] Generating unified privacy & compliance report...")
        
        # Privacy audit
        privacy_report = self.full_privacy_audit()
        
        # Load config if exists
        config_path = self.workspace_root / 'privacy_config.json'
        compliance_report = {}
        
        if config_path.exists():
            compliance_report = self.check_compliance(config_path)
        else:
            print("[!] No privacy_config.json found - skipping compliance checks")
        
        # Unified report
        report = {
            'timestamp': str(Path.ctime(Path(__file__))),
            'workspace': str(self.workspace_root),
            'privacy_audit': privacy_report,
            'compliance': compliance_report,
            'recommendations': self._generate_recommendations(privacy_report, compliance_report)
        }
        
        with open(output_path, 'w') as f:
            json.dump(report, f, indent=2)
        
        print(f"[+] Report written to {output_path}")
        return report
    
    def _generate_recommendations(self, privacy_report: Dict, compliance_report: Dict) -> List[str]:
        """Generate actionable recommendations"""
        recommendations = []
        
        # Privacy recommendations
        if privacy_report['pii_scan']['high_severity'] > 0:
            recommendations.append(
                f"CRITICAL: {privacy_report['pii_scan']['high_severity']} high-severity PII instances detected. "
                "Run auto-remediation or manually redact."
            )
        
        if privacy_report['retention']['violations'] > 0:
            recommendations.append(
                f"WARNING: {privacy_report['retention']['violations']} files exceed retention policy. "
                "Run enforce_retention_policy() to auto-delete."
            )
        
        # Compliance recommendations
        if 'gdpr' in compliance_report:
            critical_issues = compliance_report['gdpr'].get('critical_issues', [])
            for issue in critical_issues:
                recommendations.append(
                    f"GDPR NON-COMPLIANT: {issue['requirement']} - {issue['remediation']}"
                )
        
        return recommendations


def main():
    """CLI interface"""
    import argparse
    
    parser = argparse.ArgumentParser(description='Privacy & Compliance Integration')
    parser.add_argument('--workspace', type=Path, default=Path.cwd(), help='Workspace root')
    parser.add_argument('--scan', action='store_true', help='Run privacy scan')
    parser.add_argument('--compliance', action='store_true', help='Check compliance')
    parser.add_argument('--remediate', action='store_true', help='Auto-remediate PII')
    parser.add_argument('--report', type=Path, help='Generate unified report')
    parser.add_argument('--all', action='store_true', help='Run all checks')
    
    args = parser.parse_args()
    
    integration = PrivacyComplianceIntegration(args.workspace)
    
    if args.all:
        args.scan = args.compliance = True
        args.report = args.workspace / 'privacy_compliance_report.json'
    
    if args.scan:
        integration.full_privacy_audit()
    
    if args.compliance:
        config_path = args.workspace / 'privacy_config.json'
        if config_path.exists():
            integration.check_compliance(config_path)
        else:
            print(f"[!] Config not found: {config_path}")
    
    if args.remediate:
        scan_results = integration.privacy_auditor.scan_directory(args.workspace)
        integration.auto_remediate(scan_results)
    
    if args.report:
        integration.generate_unified_report(args.report)


if __name__ == '__main__':
    main()

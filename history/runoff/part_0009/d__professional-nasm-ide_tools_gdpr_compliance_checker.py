#!/usr/bin/env python3
"""
GDPR/CCPA Compliance Checker
Validates data handling practices against GDPR and CCPA requirements
"""

import json
from pathlib import Path
from typing import Dict, List, Optional
from dataclasses import dataclass, asdict
from enum import Enum


class ComplianceStatus(Enum):
    COMPLIANT = "compliant"
    NON_COMPLIANT = "non_compliant"
    PARTIAL = "partial"
    UNKNOWN = "unknown"


@dataclass
class ComplianceCheck:
    """Individual compliance check result"""
    requirement: str
    description: str
    status: ComplianceStatus
    evidence: Optional[str] = None
    remediation: Optional[str] = None


class GDPRComplianceChecker:
    """GDPR compliance validation"""
    
    def __init__(self):
        self.checks: List[ComplianceCheck] = []
    
    def check_data_minimization(self, config: dict) -> ComplianceCheck:
        """Article 5(1)(c) - Data minimization"""
        # Check if only necessary data is collected
        status = ComplianceStatus.UNKNOWN
        evidence = None
        
        if 'data_collection' in config:
            collected_fields = config['data_collection'].get('fields', [])
            necessary_fields = config['data_collection'].get('necessary_fields', [])
            
            if set(collected_fields) == set(necessary_fields):
                status = ComplianceStatus.COMPLIANT
                evidence = "Only necessary data fields are collected"
            else:
                status = ComplianceStatus.NON_COMPLIANT
                extra = set(collected_fields) - set(necessary_fields)
                evidence = f"Collecting unnecessary fields: {extra}"
        
        check = ComplianceCheck(
            requirement="GDPR Art. 5(1)(c)",
            description="Data Minimization - collect only necessary data",
            status=status,
            evidence=evidence,
            remediation="Remove unnecessary data collection fields"
        )
        self.checks.append(check)
        return check
    
    def check_storage_limitation(self, retention_days: int) -> ComplianceCheck:
        """Article 5(1)(e) - Storage limitation"""
        # Data should not be kept longer than necessary
        max_reasonable_retention = 365  # 1 year as baseline
        
        if retention_days <= max_reasonable_retention:
            status = ComplianceStatus.COMPLIANT
            evidence = f"Retention period ({retention_days} days) is reasonable"
        else:
            status = ComplianceStatus.PARTIAL
            evidence = f"Retention period ({retention_days} days) may be excessive"
        
        check = ComplianceCheck(
            requirement="GDPR Art. 5(1)(e)",
            description="Storage Limitation - data retention limits",
            status=status,
            evidence=evidence,
            remediation="Review and justify extended retention periods"
        )
        self.checks.append(check)
        return check
    
    def check_consent_mechanism(self, config: dict) -> ComplianceCheck:
        """Article 7 - Consent conditions"""
        status = ComplianceStatus.UNKNOWN
        evidence = None
        
        if 'consent' in config:
            has_opt_in = config['consent'].get('opt_in', False)
            has_granular = config['consent'].get('granular_consent', False)
            has_withdrawal = config['consent'].get('withdrawal_mechanism', False)
            
            if has_opt_in and has_granular and has_withdrawal:
                status = ComplianceStatus.COMPLIANT
                evidence = "Consent mechanism meets GDPR requirements"
            else:
                status = ComplianceStatus.NON_COMPLIANT
                missing = []
                if not has_opt_in:
                    missing.append("opt-in")
                if not has_granular:
                    missing.append("granular consent")
                if not has_withdrawal:
                    missing.append("withdrawal mechanism")
                evidence = f"Missing: {', '.join(missing)}"
        
        check = ComplianceCheck(
            requirement="GDPR Art. 7",
            description="Consent - opt-in, granular, withdrawable",
            status=status,
            evidence=evidence,
            remediation="Implement complete consent management system"
        )
        self.checks.append(check)
        return check
    
    def check_data_portability(self, config: dict) -> ComplianceCheck:
        """Article 20 - Right to data portability"""
        status = ComplianceStatus.UNKNOWN
        evidence = None
        
        if 'data_export' in config:
            has_export = config['data_export'].get('available', False)
            formats = config['data_export'].get('formats', [])
            
            if has_export and ('json' in formats or 'csv' in formats):
                status = ComplianceStatus.COMPLIANT
                evidence = f"Data export available in formats: {formats}"
            else:
                status = ComplianceStatus.NON_COMPLIANT
                evidence = "Data export not properly implemented"
        
        check = ComplianceCheck(
            requirement="GDPR Art. 20",
            description="Data Portability - user can export their data",
            status=status,
            evidence=evidence,
            remediation="Implement data export in machine-readable format"
        )
        self.checks.append(check)
        return check
    
    def check_right_to_erasure(self, config: dict) -> ComplianceCheck:
        """Article 17 - Right to erasure (right to be forgotten)"""
        status = ComplianceStatus.UNKNOWN
        evidence = None
        
        if 'data_deletion' in config:
            has_deletion = config['data_deletion'].get('available', False)
            automated = config['data_deletion'].get('automated', False)
            
            if has_deletion:
                status = ComplianceStatus.COMPLIANT if automated else ComplianceStatus.PARTIAL
                evidence = "Deletion available" + (" and automated" if automated else " (manual)")
            else:
                status = ComplianceStatus.NON_COMPLIANT
                evidence = "No deletion mechanism available"
        
        check = ComplianceCheck(
            requirement="GDPR Art. 17",
            description="Right to Erasure - delete user data on request",
            status=status,
            evidence=evidence,
            remediation="Implement automated data deletion mechanism"
        )
        self.checks.append(check)
        return check
    
    def check_encryption(self, config: dict) -> ComplianceCheck:
        """Article 32 - Security of processing"""
        status = ComplianceStatus.UNKNOWN
        evidence = None
        
        if 'security' in config:
            encrypted_at_rest = config['security'].get('encryption_at_rest', False)
            encrypted_in_transit = config['security'].get('encryption_in_transit', False)
            
            if encrypted_at_rest and encrypted_in_transit:
                status = ComplianceStatus.COMPLIANT
                evidence = "Data encrypted at rest and in transit"
            elif encrypted_at_rest or encrypted_in_transit:
                status = ComplianceStatus.PARTIAL
                evidence = f"Partial encryption: at_rest={encrypted_at_rest}, in_transit={encrypted_in_transit}"
            else:
                status = ComplianceStatus.NON_COMPLIANT
                evidence = "No encryption implemented"
        
        check = ComplianceCheck(
            requirement="GDPR Art. 32",
            description="Security - encryption and data protection",
            status=status,
            evidence=evidence,
            remediation="Implement encryption for data at rest and in transit"
        )
        self.checks.append(check)
        return check
    
    def generate_report(self) -> Dict:
        """Generate compliance report"""
        compliant = sum(1 for c in self.checks if c.status == ComplianceStatus.COMPLIANT)
        non_compliant = sum(1 for c in self.checks if c.status == ComplianceStatus.NON_COMPLIANT)
        partial = sum(1 for c in self.checks if c.status == ComplianceStatus.PARTIAL)
        
        overall_status = ComplianceStatus.COMPLIANT
        if non_compliant > 0:
            overall_status = ComplianceStatus.NON_COMPLIANT
        elif partial > 0:
            overall_status = ComplianceStatus.PARTIAL
        
        return {
            'overall_status': overall_status.value,
            'summary': {
                'total_checks': len(self.checks),
                'compliant': compliant,
                'non_compliant': non_compliant,
                'partial': partial,
                'unknown': len(self.checks) - compliant - non_compliant - partial
            },
            'checks': [asdict(check) for check in self.checks],
            'critical_issues': [
                asdict(check) for check in self.checks
                if check.status == ComplianceStatus.NON_COMPLIANT
            ]
        }


class CCPAComplianceChecker:
    """CCPA compliance validation"""
    
    def __init__(self):
        self.checks: List[ComplianceCheck] = []
    
    def check_notice_at_collection(self, config: dict) -> ComplianceCheck:
        """CCPA § 1798.100(b) - Notice at collection"""
        status = ComplianceStatus.UNKNOWN
        evidence = None
        
        if 'privacy_notice' in config:
            has_notice = config['privacy_notice'].get('displayed', False)
            categories_disclosed = config['privacy_notice'].get('categories_disclosed', False)
            
            if has_notice and categories_disclosed:
                status = ComplianceStatus.COMPLIANT
                evidence = "Privacy notice displayed with data categories"
            else:
                status = ComplianceStatus.NON_COMPLIANT
                evidence = "Incomplete privacy notice"
        
        check = ComplianceCheck(
            requirement="CCPA § 1798.100(b)",
            description="Notice at Collection - inform users about data collection",
            status=status,
            evidence=evidence,
            remediation="Display clear privacy notice at point of collection"
        )
        self.checks.append(check)
        return check
    
    def check_do_not_sell(self, config: dict) -> ComplianceCheck:
        """CCPA § 1798.120 - Right to opt-out of sale"""
        status = ComplianceStatus.UNKNOWN
        evidence = None
        
        if 'data_sale' in config:
            sells_data = config['data_sale'].get('sells_data', False)
            has_opt_out = config['data_sale'].get('opt_out_available', False)
            
            if not sells_data:
                status = ComplianceStatus.COMPLIANT
                evidence = "Does not sell personal information"
            elif has_opt_out:
                status = ComplianceStatus.COMPLIANT
                evidence = "Opt-out mechanism available"
            else:
                status = ComplianceStatus.NON_COMPLIANT
                evidence = "Sells data without opt-out mechanism"
        
        check = ComplianceCheck(
            requirement="CCPA § 1798.120",
            description="Do Not Sell - opt-out of data sales",
            status=status,
            evidence=evidence,
            remediation="Implement 'Do Not Sell My Personal Information' link"
        )
        self.checks.append(check)
        return check
    
    def generate_report(self) -> Dict:
        """Generate CCPA compliance report"""
        compliant = sum(1 for c in self.checks if c.status == ComplianceStatus.COMPLIANT)
        non_compliant = sum(1 for c in self.checks if c.status == ComplianceStatus.NON_COMPLIANT)
        
        return {
            'summary': {
                'total_checks': len(self.checks),
                'compliant': compliant,
                'non_compliant': non_compliant
            },
            'checks': [asdict(check) for check in self.checks]
        }


def main():
    """CLI interface"""
    import argparse
    
    parser = argparse.ArgumentParser(description='GDPR/CCPA Compliance Checker')
    parser.add_argument('config', type=Path, help='Configuration file (JSON)')
    parser.add_argument('--framework', choices=['gdpr', 'ccpa', 'both'], default='both')
    parser.add_argument('--output', type=Path, default='compliance_report.json')
    
    args = parser.parse_args()
    
    # Load config
    with open(args.config) as f:
        config = json.load(f)
    
    report = {}
    
    # Run GDPR checks
    if args.framework in ['gdpr', 'both']:
        gdpr = GDPRComplianceChecker()
        gdpr.check_data_minimization(config)
        gdpr.check_storage_limitation(config.get('retention_days', 365))
        gdpr.check_consent_mechanism(config)
        gdpr.check_data_portability(config)
        gdpr.check_right_to_erasure(config)
        gdpr.check_encryption(config)
        report['gdpr'] = gdpr.generate_report()
    
    # Run CCPA checks
    if args.framework in ['ccpa', 'both']:
        ccpa = CCPAComplianceChecker()
        ccpa.check_notice_at_collection(config)
        ccpa.check_do_not_sell(config)
        report['ccpa'] = ccpa.generate_report()
    
    # Write report
    with open(args.output, 'w') as f:
        json.dump(report, f, indent=2)
    
    print(f"Compliance report written to {args.output}")
    
    # Print summary
    if 'gdpr' in report:
        print(f"\nGDPR Compliance: {report['gdpr']['overall_status'].upper()}")
        print(f"  Checks: {report['gdpr']['summary']}")
    
    if 'ccpa' in report:
        print(f"\nCCPA Compliance:")
        print(f"  Checks: {report['ccpa']['summary']}")


if __name__ == '__main__':
    main()

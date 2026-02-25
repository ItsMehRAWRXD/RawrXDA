#!/usr/bin/env python3
"""
Specialized Data Processing Agents for BigDaddyG Orchestrator
Handles specific learning data processing tasks
"""

import asyncio
import json
import logging
import os
import re
from datetime import datetime
from pathlib import Path
from typing import Dict, List, Optional, Any
import aiofiles
import aiohttp

logger = logging.getLogger(__name__)

class LearningDataProcessor:
    """Processes learning data from D drive for insights"""
    
    def __init__(self, data_root: str = "D:/"):
        self.data_root = Path(data_root)
        self.processed_files = set()
        self.insights_cache = {}
        
    async def analyze_code_patterns(self, file_paths: List[str]) -> Dict[str, Any]:
        """Analyze code patterns across multiple files"""
        patterns = {
            "languages": {},
            "frameworks": {},
            "patterns": {},
            "complexity_metrics": {},
            "dependencies": set()
        }
        
        for file_path in file_paths:
            try:
                async with aiofiles.open(file_path, 'r', encoding='utf-8') as f:
                    content = await f.read()
                    
                file_ext = Path(file_path).suffix.lower()
                patterns["languages"][file_ext] = patterns["languages"].get(file_ext, 0) + 1
                
                # Analyze content patterns
                await self._analyze_file_content(content, file_ext, patterns)
                
            except Exception as e:
                logger.warning(f"Error analyzing {file_path}: {e}")
        
        return patterns
    
    async def _analyze_file_content(self, content: str, file_ext: str, patterns: Dict):
        """Analyze file content for patterns"""
        # Framework detection
        frameworks = {
            '.py': ['django', 'flask', 'fastapi', 'tensorflow', 'pytorch', 'pandas', 'numpy'],
            '.js': ['react', 'vue', 'angular', 'express', 'node', 'jquery'],
            '.html': ['bootstrap', 'tailwind', 'materialize'],
            '.cpp': ['boost', 'qt', 'opencv', 'eigen'],
            '.ps1': ['powershell', 'azure', 'office365']
        }
        
        if file_ext in frameworks:
            for framework in frameworks[file_ext]:
                if framework.lower() in content.lower():
                    patterns["frameworks"][framework] = patterns["frameworks"].get(framework, 0) + 1
        
        # Complexity analysis
        if file_ext in ['.py', '.js', '.cpp', '.c']:
            complexity = self._calculate_complexity(content, file_ext)
            patterns["complexity_metrics"][file_ext] = complexity
        
        # Dependency extraction
        if file_ext == '.py':
            imports = re.findall(r'^(?:from|import)\s+(\w+)', content, re.MULTILINE)
            patterns["dependencies"].update(imports)
        elif file_ext == '.js':
            requires = re.findall(r'require\([\'"]([^\'"]+)[\'"]\)', content)
            patterns["dependencies"].update(requires)
    
    def _calculate_complexity(self, content: str, file_ext: str) -> Dict[str, int]:
        """Calculate code complexity metrics"""
        lines = content.split('\n')
        non_empty_lines = [line for line in lines if line.strip()]
        
        complexity = {
            "total_lines": len(lines),
            "non_empty_lines": len(non_empty_lines),
            "functions": 0,
            "classes": 0,
            "comments": 0
        }
        
        if file_ext == '.py':
            complexity["functions"] = len(re.findall(r'^def\s+\w+', content, re.MULTILINE))
            complexity["classes"] = len(re.findall(r'^class\s+\w+', content, re.MULTILINE))
            complexity["comments"] = len([line for line in lines if line.strip().startswith('#')])
        elif file_ext == '.js':
            complexity["functions"] = len(re.findall(r'function\s+\w+|const\s+\w+\s*=\s*\(', content))
            complexity["classes"] = len(re.findall(r'class\s+\w+', content))
            complexity["comments"] = len([line for line in lines if '//' in line or line.strip().startswith('/*')])
        elif file_ext in ['.cpp', '.c']:
            complexity["functions"] = len(re.findall(r'\w+\s+\w+\s*\([^)]*\)\s*{', content))
            complexity["classes"] = len(re.findall(r'class\s+\w+', content))
            complexity["comments"] = len([line for line in lines if '//' in line or line.strip().startswith('/*')])
        
        return complexity

class DocumentAnalyzer:
    """Analyzes documentation and text files for insights"""
    
    def __init__(self):
        self.keywords_cache = {}
        self.topic_clusters = {}
        
    async def analyze_documentation(self, file_paths: List[str]) -> Dict[str, Any]:
        """Analyze documentation files for topics and insights"""
        analysis = {
            "topics": {},
            "keywords": {},
            "document_types": {},
            "content_quality": {},
            "recommendations": []
        }
        
        for file_path in file_paths:
            try:
                async with aiofiles.open(file_path, 'r', encoding='utf-8') as f:
                    content = await f.read()
                
                file_ext = Path(file_path).suffix.lower()
                analysis["document_types"][file_ext] = analysis["document_types"].get(file_ext, 0) + 1
                
                # Analyze content
                await self._analyze_document_content(content, file_path, analysis)
                
            except Exception as e:
                logger.warning(f"Error analyzing document {file_path}: {e}")
        
        return analysis
    
    async def _analyze_document_content(self, content: str, file_path: str, analysis: Dict):
        """Analyze document content for insights"""
        # Topic extraction
        topics = self._extract_topics(content)
        for topic in topics:
            analysis["topics"][topic] = analysis["topics"].get(topic, 0) + 1
        
        # Keyword extraction
        keywords = self._extract_keywords(content)
        for keyword in keywords:
            analysis["keywords"][keyword] = analysis["keywords"].get(keyword, 0) + 1
        
        # Content quality assessment
        quality_metrics = self._assess_content_quality(content, file_path)
        analysis["content_quality"][file_path] = quality_metrics
        
        # Generate recommendations
        recommendations = self._generate_recommendations(content, quality_metrics)
        analysis["recommendations"].extend(recommendations)
    
    def _extract_topics(self, content: str) -> List[str]:
        """Extract topics from content"""
        # Simple topic extraction based on common technical terms
        technical_terms = [
            'artificial intelligence', 'machine learning', 'deep learning',
            'programming', 'development', 'software engineering',
            'data analysis', 'algorithms', 'optimization',
            'web development', 'mobile development', 'cloud computing',
            'cybersecurity', 'networking', 'database',
            'automation', 'orchestration', 'deployment'
        ]
        
        topics = []
        content_lower = content.lower()
        for term in technical_terms:
            if term in content_lower:
                topics.append(term)
        
        return topics
    
    def _extract_keywords(self, content: str) -> List[str]:
        """Extract keywords from content"""
        # Simple keyword extraction
        words = re.findall(r'\b[a-zA-Z]{4,}\b', content.lower())
        word_freq = {}
        for word in words:
            word_freq[word] = word_freq.get(word, 0) + 1
        
        # Return most frequent words
        return sorted(word_freq.items(), key=lambda x: x[1], reverse=True)[:20]
    
    def _assess_content_quality(self, content: str, file_path: str) -> Dict[str, Any]:
        """Assess content quality metrics"""
        lines = content.split('\n')
        words = content.split()
        
        return {
            "word_count": len(words),
            "line_count": len(lines),
            "avg_words_per_line": len(words) / len(lines) if lines else 0,
            "has_structure": self._has_document_structure(content),
            "readability_score": self._calculate_readability(content),
            "completeness_score": self._assess_completeness(content, file_path)
        }
    
    def _has_document_structure(self, content: str) -> bool:
        """Check if document has proper structure"""
        structure_indicators = [
            r'^#+\s+',  # Headers
            r'^\d+\.\s+',  # Numbered lists
            r'^\*\s+',  # Bullet points
            r'```',  # Code blocks
            r'\[.*\]\(.*\)'  # Links
        ]
        
        for pattern in structure_indicators:
            if re.search(pattern, content, re.MULTILINE):
                return True
        return False
    
    def _calculate_readability(self, content: str) -> float:
        """Calculate simple readability score"""
        sentences = re.split(r'[.!?]+', content)
        words = content.split()
        
        if not sentences or not words:
            return 0.0
        
        avg_words_per_sentence = len(words) / len(sentences)
        avg_syllables_per_word = self._estimate_syllables(words)
        
        # Simple readability formula
        readability = 206.835 - (1.015 * avg_words_per_sentence) - (84.6 * avg_syllables_per_word)
        return max(0, min(100, readability))
    
    def _estimate_syllables(self, words: List[str]) -> float:
        """Estimate syllables per word"""
        total_syllables = 0
        for word in words:
            # Simple syllable estimation
            vowels = len(re.findall(r'[aeiouy]', word.lower()))
            total_syllables += max(1, vowels)
        
        return total_syllables / len(words) if words else 0
    
    def _assess_completeness(self, content: str, file_path: str) -> float:
        """Assess document completeness"""
        score = 0.0
        
        # Check for essential sections
        if 'introduction' in content.lower() or 'overview' in content.lower():
            score += 0.2
        if 'conclusion' in content.lower() or 'summary' in content.lower():
            score += 0.2
        if 'example' in content.lower() or 'usage' in content.lower():
            score += 0.2
        if 'code' in content.lower() or '```' in content:
            score += 0.2
        if len(content) > 500:  # Minimum length
            score += 0.2
        
        return score
    
    def _generate_recommendations(self, content: str, quality_metrics: Dict) -> List[str]:
        """Generate improvement recommendations"""
        recommendations = []
        
        if quality_metrics["readability_score"] < 60:
            recommendations.append("Improve readability with shorter sentences and simpler words")
        
        if not quality_metrics["has_structure"]:
            recommendations.append("Add document structure with headers and sections")
        
        if quality_metrics["completeness_score"] < 0.6:
            recommendations.append("Add missing sections like introduction, examples, or conclusion")
        
        if quality_metrics["word_count"] < 200:
            recommendations.append("Expand content with more detailed explanations")
        
        return recommendations

class InsightGenerator:
    """Generates insights from processed learning data"""
    
    def __init__(self):
        self.insight_templates = {
            "code_quality": "Code quality analysis shows {metric} with {recommendation}",
            "learning_progress": "Learning progress indicates {trend} in {domain}",
            "skill_gaps": "Identified skill gaps in {areas} with suggested learning paths",
            "project_patterns": "Project patterns reveal {insight} with {action_item}"
        }
    
    async def generate_learning_insights(self, processed_data: Dict[str, Any]) -> List[Dict[str, Any]]:
        """Generate insights from processed learning data"""
        insights = []
        
        # Code analysis insights
        if "code_patterns" in processed_data:
            code_insights = await self._analyze_code_insights(processed_data["code_patterns"])
            insights.extend(code_insights)
        
        # Documentation insights
        if "documentation_analysis" in processed_data:
            doc_insights = await self._analyze_documentation_insights(processed_data["documentation_analysis"])
            insights.extend(doc_insights)
        
        # Learning progress insights
        progress_insights = await self._analyze_learning_progress(processed_data)
        insights.extend(progress_insights)
        
        return insights
    
    async def _analyze_code_insights(self, code_patterns: Dict) -> List[Dict[str, Any]]:
        """Analyze code patterns for insights"""
        insights = []
        
        # Language diversity insight
        languages = code_patterns.get("languages", {})
        if len(languages) > 3:
            insights.append({
                "type": "skill_diversity",
                "message": f"High language diversity detected: {len(languages)} languages",
                "recommendation": "Consider focusing on 2-3 core languages for deeper expertise",
                "priority": "medium"
            })
        
        # Framework usage insight
        frameworks = code_patterns.get("frameworks", {})
        if frameworks:
            top_framework = max(frameworks.items(), key=lambda x: x[1])
            insights.append({
                "type": "framework_expertise",
                "message": f"Primary framework: {top_framework[0]} ({top_framework[1]} occurrences)",
                "recommendation": "Consider exploring related frameworks or advanced features",
                "priority": "high"
            })
        
        # Complexity insight
        complexity = code_patterns.get("complexity_metrics", {})
        if complexity:
            for lang, metrics in complexity.items():
                if metrics.get("functions", 0) > 50:
                    insights.append({
                        "type": "code_complexity",
                        "message": f"High function count in {lang} files: {metrics['functions']} functions",
                        "recommendation": "Consider refactoring into smaller, focused modules",
                        "priority": "high"
                    })
        
        return insights
    
    async def _analyze_documentation_insights(self, doc_analysis: Dict) -> List[Dict[str, Any]]:
        """Analyze documentation for insights"""
        insights = []
        
        # Topic coverage insight
        topics = doc_analysis.get("topics", {})
        if topics:
            top_topics = sorted(topics.items(), key=lambda x: x[1], reverse=True)[:3]
            insights.append({
                "type": "learning_focus",
                "message": f"Primary learning topics: {', '.join([t[0] for t in top_topics])}",
                "recommendation": "Consider exploring related topics for broader knowledge",
                "priority": "medium"
            })
        
        # Content quality insight
        quality = doc_analysis.get("content_quality", {})
        if quality:
            avg_readability = sum(q.get("readability_score", 0) for q in quality.values()) / len(quality)
            if avg_readability < 70:
                insights.append({
                    "type": "documentation_quality",
                    "message": f"Average readability score: {avg_readability:.1f}",
                    "recommendation": "Improve documentation clarity and structure",
                    "priority": "medium"
                })
        
        return insights
    
    async def _analyze_learning_progress(self, processed_data: Dict) -> List[Dict[str, Any]]:
        """Analyze learning progress patterns"""
        insights = []
        
        # File activity patterns
        recent_files = processed_data.get("recent_activity", [])
        if len(recent_files) > 10:
            insights.append({
                "type": "activity_level",
                "message": f"High activity level: {len(recent_files)} recent files",
                "recommendation": "Consider organizing projects into focused learning paths",
                "priority": "low"
            })
        
        # Skill development patterns
        languages = processed_data.get("languages_used", [])
        if len(languages) > 5:
            insights.append({
                "type": "skill_breadth",
                "message": f"Working with {len(languages)} different technologies",
                "recommendation": "Consider deepening expertise in core technologies",
                "priority": "medium"
            })
        
        return insights

# Example usage
async def process_learning_data():
    """Example of processing learning data with specialized agents"""
    
    # Initialize processors
    code_processor = LearningDataProcessor()
    doc_analyzer = DocumentAnalyzer()
    insight_generator = InsightGenerator()
    
    # Get sample files (replace with actual file paths)
    sample_files = [
        "D:/BigDaddyG-Response-generate-20251019-055334.txt",
        "D:/agent.js",
        "D:/ai_copilot.py"
    ]
    
    # Process code patterns
    code_patterns = await code_processor.analyze_code_patterns(sample_files)
    logger.info(f"Code patterns analyzed: {len(code_patterns)}")
    
    # Process documentation
    doc_analysis = await doc_analyzer.analyze_documentation(sample_files)
    logger.info(f"Documentation analyzed: {len(doc_analysis)}")
    
    # Generate insights
    processed_data = {
        "code_patterns": code_patterns,
        "documentation_analysis": doc_analysis,
        "recent_activity": sample_files,
        "languages_used": list(code_patterns.get("languages", {}).keys())
    }
    
    insights = await insight_generator.generate_learning_insights(processed_data)
    
    # Output results
    for insight in insights:
        logger.info(f"Insight: {insight['message']}")
        logger.info(f"Recommendation: {insight['recommendation']}")

if __name__ == "__main__":
    asyncio.run(process_learning_data())

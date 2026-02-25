// eXplainable AI (XAI) Template for JavaScript/Node.js
// This template provides tools for interpreting and explaining machine learning models
// Supports feature importance, model interpretation, and explanation generation

const tf = require('@tensorflow/tfjs-node');
const fs = require('fs');
const path = require('path');

class XAIAnalyzer {
    constructor(model, featureNames, targetName = "target") {
        this.model = model;
        this.featureNames = featureNames;
        this.targetName = targetName;
        this.X_train = null;
        this.X_test = null;
        this.y_train = null;
        this.y_test = null;
    }
    
    async prepareData(X, y, testSize = 0.2, randomState = 42) {
        // Simple train-test split
        const splitIndex = Math.floor(X.length * (1 - testSize));
        this.X_train = X.slice(0, splitIndex);
        this.X_test = X.slice(splitIndex);
        this.y_train = y.slice(0, splitIndex);
        this.y_test = y.slice(splitIndex);
        
        console.log(`Data split: ${this.X_train.length} train, ${this.X_test.length} test samples`);
    }
    
    async evaluateModel() {
        const trainPred = await this.model.predict(tf.tensor2d(this.X_train)).data();
        const testPred = await this.model.predict(tf.tensor2d(this.X_test)).data();
        
        const trainAcc = this.calculateAccuracy(this.y_train, Array.from(trainPred));
        const testAcc = this.calculateAccuracy(this.y_test, Array.from(testPred));
        
        console.log(`Training Accuracy: ${trainAcc.toFixed(4)}`);
        console.log(`Testing Accuracy: ${testAcc.toFixed(4)}`);
        
        return { trainAcc, testAcc };
    }
    
    calculateAccuracy(yTrue, yPred) {
        let correct = 0;
        for (let i = 0; i < yTrue.length; i++) {
            if (Math.round(yPred[i]) === yTrue[i]) {
                correct++;
            }
        }
        return correct / yTrue.length;
    }
    
    async analyzeFeatureImportance(topN = 10) {
        console.log(`\nTop ${topN} Most Important Features:`);
        
        // For TensorFlow models, we'll use gradient-based importance
        const importance = await this.calculateGradientImportance();
        
        // Create feature importance array
        const featureImportance = this.featureNames.map((name, index) => ({
            feature: name,
            importance: importance[index]
        })).sort((a, b) => b.importance - a.importance);
        
        console.log(featureImportance.slice(0, topN));
        
        // Save feature importance to file
        this.saveFeatureImportance(featureImportance);
        
        return featureImportance;
    }
    
    async calculateGradientImportance() {
        // Calculate gradients for feature importance
        const importance = [];
        
        for (let i = 0; i < this.featureNames.length; i++) {
            // Create a tensor with all zeros except for feature i
            const testInput = tf.zeros([1, this.featureNames.length]);
            const testInputModified = testInput.clone();
            testInputModified[0][i] = 1;
            
            // Calculate gradient
            const grad = tf.grad(x => this.model.predict(x))(testInputModified);
            const gradValue = await grad.data();
            importance.push(Math.abs(gradValue[0]));
            
            testInput.dispose();
            testInputModified.dispose();
            grad.dispose();
        }
        
        return importance;
    }
    
    saveFeatureImportance(featureImportance) {
        const data = {
            timestamp: new Date().toISOString(),
            featureImportance: featureImportance
        };
        
        fs.writeFileSync('feature_importance.json', JSON.stringify(data, null, 2));
        console.log('Feature importance saved to feature_importance.json');
    }
    
    async explainWithGradients(sampleIndex = 0) {
        console.log(`\n=== Gradient-based Explanation for Sample ${sampleIndex} ===`);
        
        const sample = tf.tensor2d([this.X_test[sampleIndex]]);
        const prediction = await this.model.predict(sample).data();
        
        console.log(`Prediction: ${prediction[0]}`);
        
        // Calculate gradients
        const gradients = tf.grad(x => this.model.predict(x))(sample);
        const gradValues = await gradients.data();
        
        console.log('Feature contributions:');
        this.featureNames.forEach((name, index) => {
            console.log(`${name}: ${gradValues[index].toFixed(4)}`);
        });
        
        sample.dispose();
        gradients.dispose();
        
        return gradValues;
    }
    
    async generateGlobalExplanations() {
        console.log('\n=== Global Model Explanations ===');
        
        const importance = await this.analyzeFeatureImportance();
        
        // Generate summary statistics
        const summary = {
            totalFeatures: this.featureNames.length,
            topFeatures: importance.slice(0, 5),
            modelPerformance: await this.evaluateModel()
        };
        
        console.log('Global Summary:', summary);
        
        return {
            featureImportance: importance,
            summary: summary
        };
    }
    
    async createExplanationReport(sampleIndices = [0, 1, 2]) {
        console.log('\n=== Comprehensive XAI Report ===');
        
        const report = {
            timestamp: new Date().toISOString(),
            modelPerformance: await this.evaluateModel(),
            globalExplanations: await this.generateGlobalExplanations(),
            localExplanations: {}
        };
        
        // Generate local explanations for specified samples
        for (const idx of sampleIndices) {
            if (idx < this.X_test.length) {
                console.log(`\n--- Local Explanation for Sample ${idx} ---`);
                report.localExplanations[idx] = await this.explainWithGradients(idx);
            }
        }
        
        // Save report
        fs.writeFileSync('xai_report.json', JSON.stringify(report, null, 2));
        console.log('\nXAI report saved to xai_report.json');
        
        return report;
    }
}

// Example usage and demonstration
async function demonstrateXAI() {
    console.log('=== XAI Template Demonstration ===');
    
    // Create a simple neural network model
    const model = tf.sequential({
        layers: [
            tf.layers.dense({ inputShape: [10], units: 64, activation: 'relu' }),
            tf.layers.dense({ units: 32, activation: 'relu' }),
            tf.layers.dense({ units: 1, activation: 'sigmoid' })
        ]
    });
    
    // Compile the model
    model.compile({
        optimizer: 'adam',
        loss: 'binaryCrossentropy',
        metrics: ['accuracy']
    });
    
    // Generate sample data
    const X = Array.from({ length: 1000 }, () => 
        Array.from({ length: 10 }, () => Math.random())
    );
    const y = X.map(row => row.reduce((sum, val) => sum + val, 0) > 5 ? 1 : 0);
    
    const featureNames = Array.from({ length: 10 }, (_, i) => `feature_${i}`);
    
    // Train the model
    console.log('Training model...');
    await model.fit(tf.tensor2d(X), tf.tensor1d(y), {
        epochs: 10,
        batchSize: 32,
        validationSplit: 0.2,
        verbose: 0
    });
    
    // Initialize XAI analyzer
    const analyzer = new XAIAnalyzer(model, featureNames);
    await analyzer.prepareData(X, y);
    
    // Generate comprehensive explanations
    const report = await analyzer.createExplanationReport();
    
    console.log('\n=== XAI Analysis Complete ===');
    console.log('Generated files:');
    console.log('- feature_importance.json');
    console.log('- xai_report.json');
    
    return report;
}

// Export for use in other modules
module.exports = { XAIAnalyzer, demonstrateXAI };

// Run demonstration if this file is executed directly
if (require.main === module) {
    demonstrateXAI()
        .then(report => {
            console.log('\nXAI analysis completed successfully!');
            console.log('Report summary:', Object.keys(report));
        })
        .catch(error => {
            console.error('Error during XAI analysis:', error);
        });
}

import json
import boto3
import logging

# Configure logging
logger = logging.getLogger()
logger.setLevel(logging.INFO)

def lambda_handler(event, context):
    """Zero-cost LLM Lambda function"""
    
    # Log the incoming event
    logger.info(f"Received event: {json.dumps(event)}")
    
    # Get prompt from request
    try:
        body = json.loads(event.get('body', '{}'))
        prompt = body.get('prompt', 'Hello')
        if not isinstance(prompt, str) or not prompt.strip():
            raise ValueError("Invalid prompt: Prompt must be a non-empty string.")
    except Exception as e:
        logger.error(f"Error parsing request body: {str(e)}")
        return {
            'statusCode': 400,
            'headers': {
                'Content-Type': 'application/json',
                'Access-Control-Allow-Origin': '*'
            },
            'body': json.dumps({'error': 'Invalid request body.'})
        }
    
    # Call Bedrock (free tier)
    try:
        bedrock = boto3.client('bedrock-runtime')
        response = bedrock.invoke_model(
            modelId='amazon.titan-text-lite-v1',
            body=json.dumps({
                'inputText': prompt,
                'textGenerationConfig': {
                    'maxTokenCount': 100,
                    'temperature': 0.7
                }
            })
        )
        
        result = json.loads(response['body'].read())
        output = result['results'][0]['outputText']
        logger.info(f"Model response: {output}")
        
    except Exception as e:
        logger.error(f"Error invoking Bedrock model: {str(e)}")
        output = f"Error: {str(e)}"
    
    # Return the response
    return {
        'statusCode': 200,
        'headers': {
            'Content-Type': 'application/json',
            'Access-Control-Allow-Origin': '*'
        },
        'body': json.dumps({'response': output})
    }

# Main application
def calculate(x, y):
    result = x + y
    return result

def process_data(data):
    # TODO: Add error handling
    processed = [item * 2 for item in data]
    return processed

if __name__ == "__main__":
    data = [1, 2, 3, 4, 5]
    result = process_data(data)
    print(result)

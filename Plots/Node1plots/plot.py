import matplotlib.pyplot as plt
import seaborn as sns
import pandas as pd

# Read data from the first file
file_path1 = r'C:\Users\mahon\Desktop\Videolatency\Node1\latency1\updated_file1.txt'
with open(file_path1, 'r') as file:
    node1_processing_latency = [float(line.strip()) for line in file.readlines()]

# Read data from the second file
file_path2 = r'C:\Users\mahon\Desktop\Videolatency\nonRealtime\node1\pythonProject2\processing_latency1.txt'
with open(file_path2, 'r') as file:
    node1_non_realtime_latency = [float(line.strip()) for line in file.readlines()]

# Create a DataFrame from the data
data = {
    'Node': ['Node1'] * len(node1_processing_latency) + ['Node1 Non Real Time'] * len(node1_non_realtime_latency),
    'Latency': node1_processing_latency + node1_non_realtime_latency
}

df = pd.DataFrame(data)

# Plotting box plot
plt.figure(figsize=(10, 6))
sns.boxplot(x='Node', y='Latency', data=df)
plt.title('Box Plot of Node1 Processing Latency and Non Real Time Latency')
plt.show()

# Plotting cat plot
plt.figure(figsize=(10, 6))
sns.catplot(x='Node', y='Latency', data=df, kind='swarm')
plt.title('Categorical Plot of Node1 Processing Latency and Non Real Time Latency')
plt.show()

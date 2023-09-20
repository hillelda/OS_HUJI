import matplotlib.pyplot as plt

import matplotlib.pyplot as plt

# x-coordinates of left sides of bars
left = [1, 2, 3, 4, 5,6,7,8,9]

# heights of bars
height = [0.495, 2.123,481.61, 0.492 ,2.124, 481.75, 4.73, 19.46,869.6]

# labels for bars
tick_label = ['Computer operation', 'Container - operation','VM - operation',
              'Computer function', 'Container - function', 'VM function',
              'Computer - trap', 'Container - trap', 'VM trap']

# plotting a bar chart
plt.bar(left, height, tick_label=tick_label,
        width=0.8, color=['red', 'green','Blue','red', 'green','Blue','red', 'green','Blue'])

# naming the x-axis
plt.xlabel('Platform & Operation')
# naming the y-axis
plt.ylabel('Time is NanoSeconds')
# plot title
plt.title('Platform & Operation - Time in NanoSeconds')
plt.xticks(rotation=14, ha="right", size=5)
plt.yscale("log", base=2)

# function to show the plot
plt.show()

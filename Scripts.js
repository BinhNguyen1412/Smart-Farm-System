const apiUrl = 'http://192.168.141.172:8080/data_endpoint'; // Ensure the correct API endpoint

// Function to fetch data from the API and update the interface
function fetchData() {
    fetch(apiUrl)
        .then(response => response.json())
        .then(data => {
            // Update the data into HTML elements
            document.getElementById('temperature').textContent = `${data.temperature}°C`;
            document.getElementById('humidity').textContent = `${data.humidity}%`;
            document.getElementById('air-quality').textContent = `${data.air_quality} ppm`;
            document.getElementById('water-level').textContent = data.water_level === 1 ? 'High' : 'Low';

            // Update the chart
            updateChart(data);
        })
        .catch(error => console.error('Error fetching data:', error));
}

// Update the chart
let chart = null;

function updateChart(data) {
    const ctx = document.getElementById('sensorChart').getContext('2d');
    if (chart) {
        chart.destroy(); // Remove the old chart before redrawing
    }

    chart = new Chart(ctx, {
        type: 'line',
        data: {
            labels: ['Temperature', 'Humidity', 'Air Quality', 'Water Level'],
            datasets: [{
                label: 'Sensor Data',
                data: [data.temperature, data.humidity, data.air_quality, data.water_level],
                borderColor: '#4CAF50',
                backgroundColor: 'rgba(76, 175, 80, 0.2)',
                borderWidth: 2
            }]
        },
        options: {
            scales: {
                y: {
                    beginAtZero: true
                }
            }
        }
    });
}

// Call fetchData every 5 seconds to auto-update
setInterval(fetchData, 1000);

// Call the function to fetch data as soon as the page loads
fetchData();

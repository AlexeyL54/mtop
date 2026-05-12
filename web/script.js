/**
 * @fileoverview Frontend JavaScript for System Monitor
 * Fetches and displays system metrics in real-time
 */

let updateInterval = null;
const UPDATE_INTERVAL_MS = 2000;

/**
 * Format memory size from KB to human readable
 * @param {number} kb - Memory in kilobytes
 * @returns {string} Formatted string
 */
function formatMemory(kb) {
  if (kb >= 1024 * 1024) {
    return (kb / (1024 * 1024)).toFixed(2) + ' GB';
  } else if (kb >= 1024) {
    return (kb / 1024).toFixed(2) + ' MB';
  }
  return kb + ' KB';
}

/**
 * Update the update time display
 */
function updateTime() {
  const now = new Date();
  const timeStr = now.toLocaleTimeString('en-US', {
    hour: '2-digit',
    minute: '2-digit',
    second: '2-digit'
  });
  const timeElement = document.getElementById('update-time');
  if (timeElement) {
    timeElement.textContent = `Last update: ${timeStr}`;
  }
}

/**
 * Fetch and display memory information
 */
async function updateMemory() {
  try {
    const response = await fetch('/api/memory');
    const data = await response.json();

    const total = data.MemTotal || 0;
    const available = data.MemAvailable || 0;
    const free = data.MemFree || 0;
    const cached = data.Cached || 0;

    const used = total - available;
    const usedPercent = total > 0 ? (used / total * 100).toFixed(1) : 0;

    // Update memory stats
    const memTotalEl = document.getElementById('mem-total');
    const memAvailableEl = document.getElementById('mem-available');
    const memFreeEl = document.getElementById('mem-free');
    const memCachedEl = document.getElementById('mem-cached');

    if (memTotalEl) memTotalEl.textContent = formatMemory(total);
    if (memAvailableEl) memAvailableEl.textContent = formatMemory(available);
    if (memFreeEl) memFreeEl.textContent = formatMemory(free);
    if (memCachedEl) memCachedEl.textContent = formatMemory(cached);

    // Update progress bar
    const memBar = document.getElementById('mem-usage-bar');
    const memPercentEl = document.getElementById('mem-usage-percent');

    if (memBar) {
      memBar.style.width = usedPercent + '%';
      memBar.textContent = usedPercent + '%';
    }
    if (memPercentEl) memPercentEl.textContent = usedPercent;

  } catch (error) {
    console.error('Error fetching memory data:', error);
  }
}

/**
 * Fetch and display CPU information
 */
async function updateCPU() {
  try {
    const response = await fetch('/api/cpu');
    const data = await response.json();

    if (!data.cores || data.cores.length === 0) {
      console.warn('No CPU data received');
      return;
    }

    let totalLoad = 0;
    const coresList = document.getElementById('cpu-cores-list');

    if (!coresList) return;

    coresList.innerHTML = '';

    data.cores.forEach(core => {
      totalLoad += core.load;
      const coreDiv = document.createElement('div');
      coreDiv.className = 'cpu-core';
      coreDiv.innerHTML = `
                <div class="core-id">Core ${core.id}</div>
                <div class="core-load">${core.load.toFixed(1)}%</div>
                <div class="core-bar">
                    <div class="core-bar-fill" style="width: ${Math.min(core.load, 100)}%"></div>
                </div>
            `;
      coresList.appendChild(coreDiv);
    });

    const avgLoad = data.total_cores > 0 ? (totalLoad / data.total_cores).toFixed(1) : 0;
    const cpuAvgEl = document.getElementById('cpu-avg');
    if (cpuAvgEl) {
      cpuAvgEl.textContent = avgLoad + '%';
    }

  } catch (error) {
    console.error('Error fetching CPU data:', error);
  }
}

/**
 * Filter processes based on search input
 */
function filterProcesses() {
  const searchTerm = document.getElementById('process-search')?.value.toLowerCase() || '';
  const rows = document.querySelectorAll('#processes-body tr');

  let visibleCount = 0;
  rows.forEach(row => {
    const name = row.querySelector('td:nth-child(2)')?.textContent.toLowerCase() || '';
    const pid = row.querySelector('td:nth-child(1)')?.textContent || '';

    if (name.includes(searchTerm) || pid.includes(searchTerm)) {
      row.style.display = '';
      visibleCount++;
    } else {
      row.style.display = 'none';
    }
  });

  const processCountEl = document.getElementById('process-count');
  if (processCountEl) {
    processCountEl.textContent = `${visibleCount} processes (${rows.length} total)`;
  }
}

/**
 * Fetch and display process list
 */
async function updateProcesses() {
  try {
    const response = await fetch('/api/processes');
    const data = await response.json();

    const tbody = document.getElementById('processes-body');
    if (!tbody) return;

    tbody.innerHTML = '';

    // Sort by memory usage (RSS)
    const processes = data.processes || [];
    processes.sort((a, b) => b.vmRss - a.vmRss);

    // Show top 100 processes to avoid overwhelming the browser
    const displayProcesses = processes.slice(0, 100);

    displayProcesses.forEach(proc => {
      const row = tbody.insertRow();
      const stateClass = getStateClass(proc.state);

      row.innerHTML = `
                <td>${proc.pid}</td>
                <td style="font-family: monospace;">${proc.name || '<unknown>'}</td>
                <td><span class="process-state ${stateClass}">${proc.state}</span></td>
                <td>${formatMemory(proc.vmSize)}</td>
                <td>${formatMemory(proc.vmRss)}</td>
                <td>${proc.threads}</td>
            `;
    });

    const processCountEl = document.getElementById('process-count');
    if (processCountEl) {
      processCountEl.textContent = `${displayProcesses.length} processes (${processes.length} total)`;
    }

    // Re-apply filter
    filterProcesses();

  } catch (error) {
    console.error('Error fetching process data:', error);
    const tbody = document.getElementById('processes-body');
    if (tbody) {
      tbody.innerHTML = '<tr><td colspan="6" class="loading">Error loading processes</td></tr>';
    }
  }
}

/**
 * Get CSS class for process state
 * @param {string} state - Process state character
 * @returns {string} CSS class name
 */
function getStateClass(state) {
  switch (state) {
    case 'R': return 'state-R';
    case 'S': return 'state-S';
    case 'D': return 'state-D';
    case 'Z': return 'state-Z';
    case 'T': return 'state-T';
    default: return '';
  }
}

/**
 * Update all system metrics
 */
async function updateAll() {
  await Promise.all([
    updateMemory(),
    updateCPU(),
    updateProcesses()
  ]);
  updateTime();
}

/**
 * Sort table by column
 * @param {number} column - Column index to sort by
 */
function sortTable(column) {
  const table = document.getElementById('processes-table');
  const tbody = table.querySelector('tbody');
  const rows = Array.from(tbody.querySelectorAll('tr'));

  const isNumeric = column === 0 || column === 3 || column === 4 || column === 5;

  rows.sort((a, b) => {
    let aVal = a.cells[column].textContent;
    let bVal = b.cells[column].textContent;

    if (isNumeric) {
      // Extract numbers from formatted strings
      aVal = parseFloat(aVal) || 0;
      bVal = parseFloat(bVal) || 0;
      return bVal - aVal;
    }

    return aVal.localeCompare(bVal);
  });

  rows.forEach(row => tbody.appendChild(row));
}

/**
 * Initialize the application
 */
function init() {
  console.log('Initializing System Monitor...');
  updateAll();
  updateInterval = setInterval(updateAll, UPDATE_INTERVAL_MS);

  // Setup search event listener
  const searchInput = document.getElementById('process-search');
  if (searchInput) {
    searchInput.addEventListener('input', filterProcesses);
  }

  // Setup column sorting
  const headers = document.querySelectorAll('th');
  headers.forEach((header, index) => {
    header.style.cursor = 'pointer';
    header.addEventListener('click', () => sortTable(index));
  });
}

// Start the application when page loads
if (document.readyState === 'loading') {
  document.addEventListener('DOMContentLoaded', init);
} else {
  init();
}

// Cleanup on page unload
window.addEventListener('beforeunload', () => {
  if (updateInterval) {
    clearInterval(updateInterval);
  }
});

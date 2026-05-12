"use strict";
/**
 * @fileoverview Frontend TypeScript для System Monitor
 * Получает и отображает системные метрики в реальном времени
 */
/** Константы приложения */
const UPDATE_INTERVAL_MS = 2000; // Интервал обновления данных (2 секунды)
let updateInterval = null; // ID таймера для очистки при размонтировании
/**
 * Форматирует размер памяти из KB в читаемый вид
 * @param kb - Размер в килобайтах
 * @returns Отформатированная строка (например, "1.5 GB")
 */
function formatMemory(kb) {
    if (kb >= 1024 * 1024) {
        return (kb / (1024 * 1024)).toFixed(2) + ' GB';
    }
    else if (kb >= 1024) {
        return (kb / 1024).toFixed(2) + ' MB';
    }
    return kb + ' KB';
}
/**
 * Обновляет отображение времени последнего обновления
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
 * Получает и отображает информацию о памяти
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
        const usedPercent = total > 0 ? (used / total * 100).toFixed(1) : '0';
        // Обновляем текстовые поля статистики памяти
        const memTotalEl = document.getElementById('mem-total');
        const memAvailableEl = document.getElementById('mem-available');
        const memFreeEl = document.getElementById('mem-free');
        const memCachedEl = document.getElementById('mem-cached');
        if (memTotalEl)
            memTotalEl.textContent = formatMemory(total);
        if (memAvailableEl)
            memAvailableEl.textContent = formatMemory(available);
        if (memFreeEl)
            memFreeEl.textContent = formatMemory(free);
        if (memCachedEl)
            memCachedEl.textContent = formatMemory(cached);
        // Обновляем прогресс-бар использования памяти
        const memBar = document.getElementById('mem-usage-bar');
        const memPercentEl = document.getElementById('mem-usage-percent');
        if (memBar) {
            memBar.style.width = usedPercent + '%';
            memBar.textContent = usedPercent + '%';
        }
        if (memPercentEl)
            memPercentEl.textContent = usedPercent;
    }
    catch (error) {
        console.error('Ошибка при получении данных о памяти:', error);
    }
}
/**
 * Получает и отображает информацию о CPU (все ядра)
 */
async function updateCPU() {
    try {
        const response = await fetch('/api/cpu');
        const data = await response.json();
        if (!data.cores || data.cores.length === 0) {
            console.warn('Нет данных о CPU');
            return;
        }
        let totalLoad = 0;
        const coresList = document.getElementById('cpu-cores-list');
        if (!coresList)
            return;
        // Очищаем контейнер перед добавлением новых данных
        coresList.innerHTML = '';
        // Создаём карточку для каждого ядра CPU
        data.cores.forEach((core) => {
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
        // Вычисляем и отображаем среднюю загрузку CPU
        const avgLoad = data.total_cores > 0 ? (totalLoad / data.total_cores).toFixed(1) : '0';
        const cpuAvgEl = document.getElementById('cpu-avg');
        if (cpuAvgEl) {
            cpuAvgEl.textContent = avgLoad + '%';
        }
    }
    catch (error) {
        console.error('Ошибка при получении данных о CPU:', error);
    }
}
/**
 * Фильтрует список процессов по поисковому запросу
 * Скрывает строки, не соответствующие запросу
 */
function filterProcesses() {
    const searchInput = document.getElementById('process-search');
    const searchTerm = searchInput?.value.toLowerCase() || '';
    const rows = document.querySelectorAll('#processes-body tr');
    let visibleCount = 0;
    rows.forEach((row) => {
        const nameCell = row.querySelector('td:nth-child(2)');
        const pidCell = row.querySelector('td:nth-child(1)');
        const name = nameCell?.textContent?.toLowerCase() || '';
        const pid = pidCell?.textContent || '';
        if (name.includes(searchTerm) || pid.includes(searchTerm)) {
            row.style.display = '';
            visibleCount++;
        }
        else {
            row.style.display = 'none';
        }
    });
    const processCountEl = document.getElementById('process-count');
    if (processCountEl) {
        processCountEl.textContent = `${visibleCount} процессов (${rows.length} всего)`;
    }
}
/**
 * Получает и отображает список процессов
 * Показывает топ-100 процессов по использованию памяти
 */
async function updateProcesses() {
    try {
        const response = await fetch('/api/processes');
        const data = await response.json();
        const tbody = document.getElementById('processes-body');
        if (!tbody)
            return;
        tbody.innerHTML = '';
        // Сортируем процессы по использованию физической памяти (RSS)
        const processes = data.processes || [];
        processes.sort((a, b) => b.vmRss - a.vmRss);
        // Ограничиваем до 100 процессов, чтобы не перегружать браузер
        const displayProcesses = processes.slice(0, 100);
        displayProcesses.forEach((proc) => {
            const row = tbody.insertRow();
            const stateClass = getStateClass(proc.state);
            row.innerHTML = `
                <td>${proc.pid}</td>
                <td style="font-family: monospace;">${escapeHtml(proc.name || '<unknown>')}</td>
                <td><span class="process-state ${stateClass}">${proc.state}</span></td>
                <td>${formatMemory(proc.vmSize)}</td>
                <td>${formatMemory(proc.vmRss)}</td>
                <td>${proc.threads}</td>
            `;
        });
        // Обновляем счетчик процессов
        const processCountEl = document.getElementById('process-count');
        if (processCountEl) {
            processCountEl.textContent = `${displayProcesses.length} процессов (${processes.length} всего)`;
        }
        // Повторно применяем фильтрацию (сохраняем поисковый запрос)
        filterProcesses();
    }
    catch (error) {
        console.error('Ошибка при получении данных о процессах:', error);
        const tbody = document.getElementById('processes-body');
        if (tbody) {
            tbody.innerHTML = '<td><td colspan="6" class="loading">Ошибка загрузки процессов</td></tr>';
        }
    }
}
/**
 * Экранирует HTML-символы для предотвращения XSS-атак
 * @param str - Входная строка
 * @returns Экранированная строка
 */
function escapeHtml(str) {
    if (!str)
        return '';
    return str
        .replace(/&/g, '&amp;')
        .replace(/</g, '&lt;')
        .replace(/>/g, '&gt;')
        .replace(/"/g, '&quot;')
        .replace(/'/g, '&#39;');
}
/**
 * Возвращает CSS класс для отображения состояния процесса
 * @param state - Символ состояния процесса (R, S, D, Z, T)
 * @returns Название CSS класса для стилизации
 */
function getStateClass(state) {
    switch (state) {
        case 'R': return 'state-R'; // Running
        case 'S': return 'state-S'; // Sleeping
        case 'D': return 'state-D'; // Uninterruptible sleep
        case 'Z': return 'state-Z'; // Zombie
        case 'T': return 'state-T'; // Traced/Stopped
        default: return '';
    }
}
/**
 * Обновляет все системные метрики одновременно
 * (параллельное выполнение запросов для производительности)
 */
async function updateAll() {
    await Promise.all([
        updateMemory(),
        updateCPU(),
        updateProcesses()
    ]);
    updateTime(); // Обновляем время после получения всех данных
}
/**
 * Сортирует таблицу процессов по указанной колонке
 * @param column - Индекс колонки для сортировки (0-5)
 */
function sortTable(column) {
    const table = document.getElementById('processes-table');
    if (!table)
        return;
    const tbody = table.querySelector('tbody');
    if (!tbody)
        return;
    const rows = Array.from(tbody.querySelectorAll('tr'));
    // Определяем, является ли колонка числовой
    const isNumeric = column === 0 || column === 3 || column === 4 || column === 5;
    rows.sort((a, b) => {
        let aVal = a.cells[column]?.textContent || '0';
        let bVal = b.cells[column]?.textContent || '0';
        if (isNumeric) {
            // Извлекаем числа из форматированных строк (обрезаем единицы измерения)
            const aNum = parseFloat(aVal) || 0;
            const bNum = parseFloat(bVal) || 0;
            return bNum - aNum; // Сортировка по убыванию
        }
        return aVal.localeCompare(bVal); // Лексикографическая сортировка
    });
    // Перемещаем отсортированные строки обратно в tbody
    rows.forEach((row) => tbody.appendChild(row));
}
/**
 * Инициализирует приложение:
 * - Загружает начальные данные
 * - Запускает периодическое обновление
 * - Настраивает обработчики событий (поиск, сортировка)
 */
function init() {
    console.log('Инициализация System Monitor...');
    // Первоначальная загрузка данных
    updateAll();
    // Запускаем периодическое обновление
    updateInterval = window.setInterval(updateAll, UPDATE_INTERVAL_MS);
    // Настройка обработчика поиска
    const searchInput = document.getElementById('process-search');
    if (searchInput) {
        searchInput.addEventListener('input', filterProcesses);
    }
    // Настройка обработчиков сортировки для всех заголовков таблицы
    const headers = document.querySelectorAll('th');
    headers.forEach((header, index) => {
        header.style.cursor = 'pointer';
        header.addEventListener('click', () => sortTable(index));
    });
}
/**
 * Очистка ресурсов при закрытии страницы
 * Отменяет таймер обновления, чтобы избежать утечек памяти
 */
function cleanup() {
    if (updateInterval !== null) {
        clearInterval(updateInterval);
        updateInterval = null;
    }
}
// Проверяем состояние загрузки документа
if (document.readyState === 'loading') {
    // Если DOM ещё не готов, ждём события DOMContentLoaded
    document.addEventListener('DOMContentLoaded', init);
}
else {
    // DOM уже загружен, запускаемся сразу
    init();
}
// Очищаем ресурсы при выгрузке страницы
window.addEventListener('beforeunload', cleanup);
//# sourceMappingURL=script.js.map
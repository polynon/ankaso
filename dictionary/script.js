async function loadDictionary() {
    try {
        // Fetch both JSON files
        const dictResponse = await fetch('dictionary.json');
        const dictionary = await dictResponse.json();
        
        const langResponse = await fetch('../lang/en.json');
        const translations = await langResponse.json();
        
        // Get all keys and sort alphabetically
        const allKeys = Object.keys(dictionary).sort();
        const detailsContainer = document.querySelector('.details-container');

        function renderDetails(key) {
            const selectedKey = key && dictionary[key] ? key : 'ai';
            const entry = dictionary[selectedKey];
            const transData = translations[selectedKey] || {};

            detailsContainer.innerHTML = '';

            const title = document.createElement('h2');
            title.className = 'details-title';
            title.textContent = entry.root || selectedKey;
            detailsContainer.appendChild(title);

            if (entry['o-form']) {
                const subtitle = document.createElement('p');
                subtitle.className = 'details-subtitle';
                subtitle.textContent = `O-form: ${entry['o-form']}`;
                detailsContainer.appendChild(subtitle);
            }

            const addSection = (heading, values) => {
                const section = document.createElement('div');
                section.className = 'details-section';

                const headingEl = document.createElement('h3');
                headingEl.textContent = heading;
                section.appendChild(headingEl);

                const list = document.createElement('div');
                list.className = 'details-list';

                values.forEach(value => {
                    const item = document.createElement('span');
                    item.className = 'details-item';
                    item.textContent = value;
                    list.appendChild(item);
                });

                section.appendChild(list);
                detailsContainer.appendChild(section);
            };

            if (transData.general && transData.general.length > 0) {
                addSection('General meanings', transData.general);
            }

            if (transData.root && transData.root.length > 0) {
                addSection('Root meanings', transData.root);
            }

            if (transData['o-form'] && transData['o-form'].length > 0) {
                addSection('O-form meanings', transData['o-form']);
            }

            if (transData.description) {
                const description = document.createElement('div');
                description.className = 'details-section';
                const headingEl = document.createElement('h3');
                headingEl.textContent = 'Description';
                description.appendChild(headingEl);

                const body = document.createElement('p');
                body.className = 'details-description';
                body.textContent = transData.description;
                description.appendChild(body);
                detailsContainer.appendChild(description);
            }

            if (transData.tags && transData.tags.length > 0) {
                addSection('Tags', transData.tags);
            }

            const note = document.createElement('p');
            note.className = 'details-note';
            note.innerHTML = 'Rendered from <code>dictionary.json</code> and <code>en.json</code>.';
            detailsContainer.appendChild(note);
        }

        // Function to render results
        function renderResults(keys) {
            const resultsContainer = document.getElementById('results');
            resultsContainer.innerHTML = '';
            
            keys.forEach(key => {
                const entry = dictionary[key];
                const transData = translations[key];
                
                // Create entry div
                const entryDiv = document.createElement('div');
                entryDiv.className = 'dictionary-entry';
                entryDiv.id = key;
                entryDiv.style.cursor = 'pointer';
                entryDiv.addEventListener('click', () => {
                    setActiveEntry(key);
                    scrollToElement(entryDiv);
                    window.history.replaceState(null, null, '#' + key);
                });
                
                // Create root and o-form line
                const rootLine = document.createElement('div');
                rootLine.className = 'root-line';
                
                // Create root span
                const rootSpan = document.createElement('span');
                rootSpan.className = 'root-text';
                rootSpan.textContent = entry.root || key;
                rootLine.appendChild(rootSpan);
                
                // Add o-form if present
                if (entry['o-form']) {
                    const slashSpan = document.createElement('span');
                    slashSpan.className = 'root-slash';
                    slashSpan.textContent = ' / ';
                    rootLine.appendChild(slashSpan);
                    
                    const oFormSpan = document.createElement('span');
                    oFormSpan.className = 'root-text';
                    oFormSpan.textContent = entry['o-form'];
                    rootLine.appendChild(oFormSpan);
                }
                
                entryDiv.appendChild(rootLine);
                
                // Add translations and description if present
                if (transData) {
                    const infoDiv = document.createElement('div');
                    infoDiv.className = 'entry-info';
                    
                    // Add general translations
                    if (transData.general && transData.general.length > 0) {
                        const transSpan = document.createElement('span');
                        transSpan.className = 'entry-translation';
                        transSpan.textContent = transData.general.join(', ');
                        infoDiv.appendChild(transSpan);
                    }
                    
                    // Add description if present
                    if (transData.description) {
                        const descSpan = document.createElement('span');
                        descSpan.className = 'entry-description';
                        descSpan.textContent = transData.description;
                        infoDiv.appendChild(descSpan);
                    }
                    
                    entryDiv.appendChild(infoDiv);
                }
                
                resultsContainer.appendChild(entryDiv);
            });

            restoreActiveEntry();
            renderDetails(activeEntryKey);
        }
        
        let activeEntryKey = null;

        function setActiveEntry(key) {
            const existing = document.querySelector('.dictionary-entry.active');
            if (existing) {
                existing.classList.remove('active');
            }

            activeEntryKey = key;
            renderDetails(key);

            if (!key) {
                return;
            }

            const next = document.getElementById(key);
            if (next) {
                next.classList.add('active');
            }
        }

        function restoreActiveEntry() {
            if (!activeEntryKey) {
                return;
            }

            const next = document.getElementById(activeEntryKey);
            if (next) {
                next.classList.add('active');
            } else {
                activeEntryKey = null;
            }
        }

        // Initial render of all keys
        renderResults(allKeys);
        renderDetails(activeEntryKey);
        
        // Function to scroll to element with offset
        function scrollToElement(element) {
            const header = document.querySelector('.top-bar');
            const headerHeight = header ? header.offsetHeight : 0;
            const offset = headerHeight + 300; // Account for header and some padding
            const elementTop = element.getBoundingClientRect().top + window.scrollY;
            window.scrollTo({
                top: elementTop - offset,
                behavior: 'smooth'
            });
        }
        
        // Handle initial hash
        if (window.location.hash) {
            const hash = window.location.hash.substring(1);
            setActiveEntry(hash);
            const element = document.getElementById(hash);
            if (element) {
                scrollToElement(element);
            }
        }
        
        // Handle hash changes (e.g., when clicking entries or manual navigation)
        window.addEventListener('hashchange', () => {
            if (window.location.hash) {
                const hash = window.location.hash.substring(1);
                setActiveEntry(hash);
                const element = document.getElementById(hash);
                if (element) {
                    scrollToElement(element);
                }
            }
        });
        
        // Add search functionality
        const searchInput = document.querySelector('.search-input');
        searchInput.addEventListener('input', function() {
            const query = this.value.trim().toLowerCase();
            if (query === '') {
                renderResults(allKeys);
                return;
            }
            
            // Find matching keys
            const matchingKeys = allKeys.filter(key => {
                const entry = dictionary[key];
                const transData = translations[key];
                
                // Check dictionary.json root, o-form, tags
                if ((entry.root || key).toLowerCase() === query) return true;
                if (entry['o-form'] && entry['o-form'].toLowerCase() === query) return true;
                if (entry.tags && entry.tags.some(tag => tag.toLowerCase() === query)) return true;
                
                // Check en.json description and tags
                if (transData) {
                    if (transData.description && transData.description.toLowerCase() === query) return true;
                    if (transData.tags && transData.tags.some(tag => tag.toLowerCase() === query)) return true;
                    if (transData.root && transData.root.some(term => term.toLowerCase() === query)) return true;
                    if (transData['o-form'] && transData['o-form'].some(term => term.toLowerCase() === query)) return true;
                }
                
                return false;
            });
            
            // Remove duplicates (though unlikely)
            const uniqueKeys = [...new Set(matchingKeys)];
            
            renderResults(uniqueKeys);
        });
        
    } catch (error) {
        console.error('Error loading dictionary:', error);
        document.getElementById('results').textContent = 'Error loading dictionary';
    }
}

// Load dictionary when page loads
document.addEventListener('DOMContentLoaded', loadDictionary);

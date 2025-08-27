// Global variables
let currentUser = null;
let uploadedFileId = null;

// DOM Elements
const authSection = document.getElementById('authSection');
const appSection = document.getElementById('appSection');
const uploadArea = document.getElementById('uploadArea');
const fileInput = document.getElementById('fileInput');
const uploadProgress = document.getElementById('uploadProgress');
const progressFill = document.getElementById('progressFill');
const progressText = document.getElementById('progressText');
const uploadResult = document.getElementById('uploadResult');
const filesList = document.getElementById('filesList');

// Event Listeners
document.getElementById('registerForm').addEventListener('submit', handleRegister);
document.getElementById('loginFormActual').addEventListener('submit', handleLogin);
document.getElementById('showLogin').addEventListener('click', showLogin);
document.getElementById('showRegister').addEventListener('click', showRegister);
document.getElementById('logoutBtn').addEventListener('click', handleLogout);
document.getElementById('shareBtn').addEventListener('click', handleShare);
uploadArea.addEventListener('click', () => fileInput.click());
uploadArea.addEventListener('dragover', handleDragOver);
uploadArea.addEventListener('drop', handleDrop);
fileInput.addEventListener('change', handleFileSelect);

// Show login form
function showLogin(e) {
    e.preventDefault();
    document.querySelector('.auth-form').style.display = 'none';
    document.getElementById('loginForm').style.display = 'block';
}

// Show register form
function showRegister(e) {
    e.preventDefault();
    document.getElementById('loginForm').style.display = 'none';
    document.querySelector('.auth-form').style.display = 'block';
}

// Handle drag over
function handleDragOver(e) {
    e.preventDefault();
    e.stopPropagation();
    uploadArea.style.borderColor = '#764ba2';
    uploadArea.style.backgroundColor = '#e6e9ff';
}

// Handle drop
function handleDrop(e) {
    e.preventDefault();
    e.stopPropagation();
    uploadArea.style.borderColor = '#667eea';
    uploadArea.style.backgroundColor = '#f8f9ff';
    
    if (e.dataTransfer.files.length) {
        fileInput.files = e.dataTransfer.files;
        uploadFile(e.dataTransfer.files[0]);
    }
}

// Handle file select
function handleFileSelect(e) {
    if (e.target.files.length) {
        uploadFile(e.target.files[0]);
    }
}

// Handle registration
async function handleRegister(e) {
    e.preventDefault();
    
    const username = document.getElementById('regUsername').value;
    const email = document.getElementById('regEmail').value;
    const password = document.getElementById('regPassword').value;
    
    try {
        const response = await fetch('/api/register', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({ username, email, password })
        });
        
        const result = await response.json();
        
        if (result.success) {
            showNotification('Registration successful!', 'success');
            // Auto-login after registration
            loginUser(username, password);
        } else {
            showNotification(result.message || 'Registration failed', 'error');
        }
    } catch (error) {
        showNotification('Registration failed: ' + error.message, 'error');
    }
}

// Handle login
async function handleLogin(e) {
    e.preventDefault();
    
    const username = document.getElementById('loginUsername').value;
    const password = document.getElementById('loginPassword').value;
    
    loginUser(username, password);
}

// Login user
async function loginUser(username, password) {
    try {
        const response = await fetch('/api/login', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({ username, password })
        });
        
        const result = await response.json();
        
        if (result.success) {
            currentUser = { id: result.data.user_id, username: result.data.username };
            showApp();
            loadSharedFiles();
            showNotification('Login successful!', 'success');
        } else {
            showNotification(result.message || 'Login failed', 'error');
        }
    } catch (error) {
        showNotification('Login failed: ' + error.message, 'error');
    }
}

// Handle logout
function handleLogout() {
    currentUser = null;
    authSection.style.display = 'block';
    appSection.style.display = 'none';
    showNotification('Logged out successfully', 'success');
}

// Show app interface
function showApp() {
    authSection.style.display = 'none';
    appSection.style.display = 'block';
    document.getElementById('usernameDisplay').textContent = currentUser.username;
}

// Upload file
async function uploadFile(file) {
    // Show progress
    uploadProgress.style.display = 'block';
    uploadResult.style.display = 'none';
    
    try {
        const formData = new FormData();
        formData.append('file', file);
        
        const response = await fetch('/api/upload', {
            method: 'POST',
            body: formData
        });
        
        const result = await response.json();
        
        if (result.success) {
            uploadedFileId = result.data.file_id;
            document.getElementById('fileIdDisplay').textContent = uploadedFileId;
            document.getElementById('speedDisplay').textContent = result.data.throughput_mbps.toFixed(2);
            
            // Hide progress, show result
            uploadProgress.style.display = 'none';
            uploadResult.style.display = 'block';
            
            showNotification('File uploaded successfully!', 'success');
            loadSharedFiles();
        } else {
            uploadProgress.style.display = 'none';
            showNotification(result.message || 'Upload failed', 'error');
        }
    } catch (error) {
        uploadProgress.style.display = 'none';
        showNotification('Upload failed: ' + error.message, 'error');
    }
}

// Handle share
async function handleShare() {
    const fileId = document.getElementById('fileIdInput').value;
    const recipientId = document.getElementById('recipientIdInput').value;
    
    if (!fileId || !recipientId) {
        showNotification('Please enter both File ID and Recipient ID', 'error');
        return;
    }
    
    try {
        const response = await fetch('/api/share', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({ file_id: fileId, recipient_id: parseInt(recipientId) })
        });
        
        const result = await response.json();
        
        if (result.success) {
            showNotification('File shared successfully!', 'success');
            document.getElementById('fileIdInput').value = '';
            document.getElementById('recipientIdInput').value = '';
        } else {
            showNotification(result.message || 'Failed to share file', 'error');
        }
    } catch (error) {
        showNotification('Failed to share file: ' + error.message, 'error');
    }
}

// Load shared files
async function loadSharedFiles() {
    try {
        const response = await fetch('/api/files');
        const result = await response.json();
        
        if (result.success) {
            renderFiles(result.data);
        } else {
            showNotification(result.message || 'Failed to load files', 'error');
        }
    } catch (error) {
        showNotification('Failed to load files: ' + error.message, 'error');
    }
}

// Render files list
function renderFiles(files) {
    filesList.innerHTML = '';
    
    if (!files || files.length === 0) {
        filesList.innerHTML = '<p>No shared files found.</p>';
        return;
    }
    
    files.forEach(file => {
        const fileElement = document.createElement('div');
        fileElement.className = 'file-item';
        fileElement.innerHTML = `
            <div class="file-info">
                <h3>${file.file_name || 'Unnamed File'}</h3>
                <p>From: ${file.uploader} | Shared: ${new Date(file.shared_at).toLocaleString()}</p>
                <p>File ID: ${file.file_id}</p>
            </div>
            <div class="file-actions">
                <button class="download-btn" onclick="downloadFile('${file.file_id}')">Download</button>
            </div>
        `;
        filesList.appendChild(fileElement);
    });
}

// Download file
async function downloadFile(fileId) {
    try {
        // Create a temporary link to trigger download
        const link = document.createElement('a');
        link.href = `/api/download/${fileId}`;
        link.download = fileId;
        document.body.appendChild(link);
        link.click();
        document.body.removeChild(link);
        
        showNotification('Download started!', 'success');
    } catch (error) {
        showNotification('Download failed: ' + error.message, 'error');
    }
}

// Show notification
function showNotification(message, type) {
    // Remove existing notifications
    const existing = document.querySelector('.notification');
    if (existing) {
        existing.remove();
    }
    
    const notification = document.createElement('div');
    notification.className = `notification ${type}`;
    notification.textContent = message;
    
    document.body.appendChild(notification);
    
    // Remove after 3 seconds
    setTimeout(() => {
        if (notification.parentNode) {
            notification.parentNode.removeChild(notification);
        }
    }, 3000);
}

// Initialize
document.addEventListener('DOMContentLoaded', function() {
    // Check if user is already logged in (simplified for demo)
    // In a real app, you would check session/cookies
    console.log('Red Giant File Share App initialized');
});
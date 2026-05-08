import React, { useState, useEffect } from 'react';
import {
  Box,
  Card,
  CardContent,
  Typography,
  Grid,
  Chip,
  Button,
  Dialog,
  DialogTitle,
  DialogContent,
  DialogActions,
  FormControl,
  InputLabel,
  Select,
  MenuItem,
  Switch,
  FormControlLabel,
  LinearProgress,
  Alert,
  Table,
  TableBody,
  TableCell,
  TableContainer,
  TableHead,
  TableRow,
  Paper,
  IconButton,
  Tooltip
} from '@mui/material';
import {
  Gpu as GpuIcon,
  AttachFile as AttachIcon,
  LinkOff as DetachIcon,
  Thermostat as TempIcon,
  Memory as MemoryIcon,
  Speed as SpeedIcon,
  Power as PowerIcon
} from '@mui/icons-material';

const API = '';

const GPUManager = ({ selectedVM, onVMUpdate }) => {
  const [gpus, setGpus] = useState([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState(null);
  const [attachDialog, setAttachDialog] = useState(false);
  const [selectedGPU, setSelectedGPU] = useState('');
  const [enableVGPU, setEnableVGPU] = useState(false);
  const [iommuStatus, setIommuStatus] = useState(null);

  // Load available GPUs
  const loadGPUs = async () => {
    try {
      setLoading(true);
      const response = await fetch(`${API}/api/gpu/available`);
      const data = await response.json();

      if (data.success) {
        setGpus(data.devices);
      } else {
        setError(data.message || 'Failed to load GPUs');
      }
    } catch (err) {
      setError('Failed to connect to API');
    } finally {
      setLoading(false);
    }
  };

  // Check IOMMU status
  const checkIOMMU = async () => {
    try {
      const response = await fetch(`${API}/api/gpu/check-iommu`, { method: 'POST' });
      const data = await response.json();
      setIommuStatus(data);
    } catch (err) {
      setIommuStatus({ success: false, message: 'Failed to check IOMMU status' });
    }
  };

  // Attach GPU to VM
  const attachGPU = async () => {
    if (!selectedVM || !selectedGPU) return;

    try {
      const response = await fetch(`${API}/api/vm/${selectedVM.id}/gpu/attach`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          gpu_id: selectedGPU,
          enable_vgpu: enableVGPU
        })
      });

      const data = await response.json();
      if (data.success) {
        setAttachDialog(false);
        setSelectedGPU('');
        setEnableVGPU(false);
        loadGPUs();
        if (onVMUpdate) onVMUpdate();
      } else {
        setError(data.message || 'Failed to attach GPU');
      }
    } catch (err) {
      setError('Failed to attach GPU');
    }
  };

  // Detach GPU from VM
  const detachGPU = async (gpuId) => {
    if (!selectedVM) return;

    try {
      const response = await fetch(`${API}/api/vm/${selectedVM.id}/gpu/${gpuId}`, {
        method: 'DELETE'
      });

      const data = await response.json();
      if (data.success) {
        loadGPUs();
        if (onVMUpdate) onVMUpdate();
      } else {
        setError(data.message || 'Failed to detach GPU');
      }
    } catch (err) {
      setError('Failed to detach GPU');
    }
  };

  // Get GPU metrics for VM
  const getVMMetrics = async () => {
    if (!selectedVM) return null;

    try {
      const response = await fetch(`${API}/api/vm/${selectedVM.id}/gpu`);
      const data = await response.json();
      return data.success ? data : null;
    } catch (err) {
      return null;
    }
  };

  useEffect(() => {
    loadGPUs();
    checkIOMMU();
  }, []);

  const getVendorColor = (vendor) => {
    switch (vendor) {
      case 'nvidia': return '#76b900';
      case 'amd': return '#ed1c24';
      case 'intel': return '#0071c5';
      default: return '#666';
    }
  };

  const formatMemory = (mb) => {
    if (mb >= 1024) {
      return `${(mb / 1024).toFixed(1)} GB`;
    }
    return `${mb} MB`;
  };

  return (
    <Box>
      <Typography variant="h5" gutterBottom sx={{ display: 'flex', alignItems: 'center', gap: 1 }}>
        <GpuIcon />
        GPU Management
      </Typography>

      {/* IOMMU Status */}
      {iommuStatus && (
        <Alert
          severity={iommuStatus.iommu_enabled ? "success" : "warning"}
          sx={{ mb: 2 }}
        >
          {iommuStatus.message}
        </Alert>
      )}

      {/* Error Display */}
      {error && (
        <Alert severity="error" sx={{ mb: 2 }} onClose={() => setError(null)}>
          {error}
        </Alert>
      )}

      {/* GPU List */}
      <Grid container spacing={2}>
        {gpus.map((gpu) => (
          <Grid item xs={12} md={6} key={gpu.id}>
            <Card>
              <CardContent>
                <Box display="flex" justifyContent="space-between" alignItems="center" mb={2}>
                  <Typography variant="h6" sx={{ display: 'flex', alignItems: 'center', gap: 1 }}>
                    <GpuIcon />
                    {gpu.name}
                  </Typography>
                  <Chip
                    label={gpu.vendor.toUpperCase()}
                    sx={{ bgcolor: getVendorColor(gpu.vendor) }}
                  />
                </Box>

                <Box mb={2}>
                  <Typography variant="body2" color="text.secondary">
                    ID: {gpu.id}
                  </Typography>
                  <Typography variant="body2" color="text.secondary">
                    Memory: {formatMemory(gpu.memory_mb)}
                  </Typography>
                  <Typography variant="body2" color="text.secondary">
                    Driver: {gpu.driver}
                  </Typography>
                </Box>

                {/* GPU Status */}
                <Box display="flex" gap={1} mb={2}>
                  <Chip
                    label={gpu.currently_attached ? "Attached" : "Available"}
                    color={gpu.currently_attached ? "success" : "default"}
                    size="small"
                  />
                  <Chip
                    label={gpu.supports_passthrough ? "Passthrough OK" : "No Passthrough"}
                    color={gpu.supports_passthrough ? "success" : "error"}
                    size="small"
                  />
                </Box>

                {/* Attach/Detach Actions */}
                {selectedVM && (
                  <Box>
                    {gpu.currently_attached ? (
                      <Button
                        variant="outlined"
                        color="error"
                        startIcon={<DetachIcon />}
                        onClick={() => detachGPU(gpu.id)}
                        disabled={gpu.vm_id !== selectedVM.id}
                      >
                        Detach from {selectedVM.id}
                      </Button>
                    ) : (
                      <Button
                        variant="contained"
                        startIcon={<AttachIcon />}
                        onClick={() => {
                          setSelectedGPU(gpu.id);
                          setAttachDialog(true);
                        }}
                        disabled={!gpu.supports_passthrough}
                      >
                        Attach to {selectedVM.id}
                      </Button>
                    )}
                  </Box>
                )}
              </CardContent>
            </Card>
          </Grid>
        ))}
      </Grid>

      {/* VM GPU Metrics */}
      {selectedVM && (
        <Box mt={4}>
          <Typography variant="h6" gutterBottom>
            GPU Metrics for VM: {selectedVM.id}
          </Typography>
          {/* This would show real-time GPU metrics for the selected VM */}
          <Typography variant="body2" color="text.secondary">
            GPU metrics will be displayed here when implemented
          </Typography>
        </Box>
      )}

      {/* Attach GPU Dialog */}
      <Dialog open={attachDialog} onClose={() => setAttachDialog(false)}>
        <DialogTitle>Attach GPU to VM</DialogTitle>
        <DialogContent>
          <FormControl fullWidth sx={{ mt: 2 }}>
            <InputLabel>GPU</InputLabel>
            <Select
              value={selectedGPU}
              onChange={(e) => setSelectedGPU(e.target.value)}
            >
              {gpus
                .filter(gpu => !gpu.currently_attached && gpu.supports_passthrough)
                .map(gpu => (
                <MenuItem key={gpu.id} value={gpu.id}>
                  {gpu.name} ({gpu.id})
                </MenuItem>
              ))}
            </Select>
          </FormControl>

          <FormControlLabel
            control={
              <Switch
                checked={enableVGPU}
                onChange={(e) => setEnableVGPU(e.target.checked)}
              />
            }
            label="Enable vGPU sharing"
            sx={{ mt: 2 }}
          />

          {enableVGPU && (
            <Alert severity="info" sx={{ mt: 2 }}>
              vGPU sharing allows multiple VMs to share the same GPU, but with reduced performance.
            </Alert>
          )}
        </DialogContent>
        <DialogActions>
          <Button onClick={() => setAttachDialog(false)}>Cancel</Button>
          <Button onClick={attachGPU} variant="contained">
            Attach GPU
          </Button>
        </DialogActions>
      </Dialog>
    </Box>
  );
};

export default GPUManager;
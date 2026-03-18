package com.mandnargitech.fluidlevelmonitor.ui.pump
import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import com.mandnargitech.fluidlevelmonitor.data.model.PumpState
import com.mandnargitech.fluidlevelmonitor.data.repository.DeviceRepository
import dagger.hilt.android.lifecycle.HiltViewModel
import kotlinx.coroutines.flow.*
import kotlinx.coroutines.launch
import javax.inject.Inject

data class PumpUiState(
    val pumpState: PumpState = PumpState(),
    val loading: Boolean     = false,
    val error: String?       = null,
    val pumpOnPct: Int       = 20,
    val pumpOffPct: Int      = 85,
)

@HiltViewModel
class PumpViewModel @Inject constructor(private val repo: DeviceRepository) : ViewModel() {
    private val _state = MutableStateFlow(PumpUiState())
    val uiState: StateFlow<PumpUiState> = _state.asStateFlow()
    init { refresh() }
    fun refresh() = viewModelScope.launch {
        _state.update { it.copy(loading = true) }
        repo.getPumpState()
            .onSuccess { ps -> _state.update { it.copy(pumpState = ps, loading = false) } }
            .onFailure { e  -> _state.update { it.copy(loading = false, error = e.message) } }
    }
    fun setMode(mode: String) = viewModelScope.launch { repo.setPumpState(mode); refresh() }
    fun updateOnThreshold(pct: Int)  = _state.update { it.copy(pumpOnPct = pct) }
    fun updateOffThreshold(pct: Int) = _state.update { it.copy(pumpOffPct = pct) }
}

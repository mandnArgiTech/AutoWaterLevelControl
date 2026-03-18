package com.mandnargitech.fluidlevelmonitor.ui.dashboard
import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import com.mandnargitech.fluidlevelmonitor.data.model.*
import com.mandnargitech.fluidlevelmonitor.data.repository.DeviceRepository
import dagger.hilt.android.lifecycle.HiltViewModel
import kotlinx.coroutines.flow.*
import kotlinx.coroutines.launch
import javax.inject.Inject

data class DashboardUiState(
    val loading: Boolean      = true,
    val status: DeviceStatus? = null,
    val tankState: TankState  = TankState.UNKNOWN,
    val showLowAlert: Boolean = false,
    val error: String?        = null,
)

@HiltViewModel
class DashboardViewModel @Inject constructor(private val repo: DeviceRepository) : ViewModel() {
    private val _ui = MutableStateFlow(DashboardUiState())
    val uiState: StateFlow<DashboardUiState> = _ui.asStateFlow()
    init {
        viewModelScope.launch {
            repo.statusFlow(5_000L).collect { result ->
                result.onSuccess { s ->
                    _ui.update { it.copy(
                        loading = false, status = s,
                        tankState = TankState.from(s.level.state),
                        showLowAlert = s.level.sensorOk && s.level.percentFilled < 20f,
                        error = null,
                    )}
                }.onFailure { e ->
                    _ui.update { it.copy(loading = false, error = e.message) }
                }
            }
        }
    }
    fun sendPumpCommand(cmd: String) = viewModelScope.launch { repo.setPumpState(cmd) }
}

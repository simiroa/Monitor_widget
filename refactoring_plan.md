# Refactoring Plan: Weather Module Optimization

## 1. Data Model & Utility Separation (Priority: High)

- **Objective**: Decouple data structures and helper functions from the main service.
- **Actions**:
  - Create `src/models/weather_data.h`: Move `WeatherData`, `LocationConfig`, `HourlyItem`, `DailyItem` structs here.
  - Create `src/utils/geo_utils.h/.cpp`: Move `latLonToGrid`, `findRegIdForCoords`, and `getAvailableCities` here.
  - Update `weather_service.h` to include these new headers.

## 2. Location Manager Extraction (Priority: Medium)

- **Objective**: Encapsulate location detection, storage, and management.
- **Actions**:
  - Create `src/services/location_manager.h/.cpp`.
  - Move `detectLocation`, `saveLocation`, `loadSavedLocation`, `registerCustomLocation` to this class.
  - `WeatherService` will delegate location tasks to `LocationManager`.

## 3. API Client Separation (Priority: Medium)

- **Objective**: Isolate network operations and JSON parsing.
- **Actions**:
  - Create `src/api/weather_api_client.h/.cpp`.
  - Move `fetchShortTerm`, `fetchMidLand`, `fetchMidTemp`, `fetchWarnings` and their parsing logic here.
  - This class will return structured data (`WeatherData`) or emits signals with parsed data.

## 4. UI Layout Modularization (Priority: Low)

- **Objective**: Simplify `DashboardPage`.
- **Actions**:
  - Extract the top weather card into `src/ui/widgets/weather_card_widget.h/.cpp`.
  - Extract the tabbed forecast view into `src/ui/widgets/forecast_tab_widget.h/.cpp`.
  - `DashboardPage` will become a simple container managing layout and connections.

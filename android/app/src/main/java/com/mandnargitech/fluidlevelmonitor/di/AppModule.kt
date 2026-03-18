package com.mandnargitech.fluidlevelmonitor.di
import com.mandnargitech.fluidlevelmonitor.data.api.FluidApi
import dagger.Module
import dagger.Provides
import dagger.hilt.InstallIn
import dagger.hilt.components.SingletonComponent
import okhttp3.OkHttpClient
import okhttp3.logging.HttpLoggingInterceptor
import retrofit2.Retrofit
import retrofit2.converter.gson.GsonConverterFactory
import java.util.concurrent.TimeUnit
import javax.inject.Singleton

@Module
@InstallIn(SingletonComponent::class)
object AppModule {
    @Provides @Singleton
    fun provideOkHttp(): OkHttpClient = OkHttpClient.Builder()
        .addInterceptor(HttpLoggingInterceptor().apply { level = HttpLoggingInterceptor.Level.BASIC })
        .connectTimeout(10, TimeUnit.SECONDS).readTimeout(15, TimeUnit.SECONDS).build()

    @Provides @Singleton
    fun provideRetrofit(http: OkHttpClient): Retrofit = Retrofit.Builder()
        .baseUrl("http://192.168.4.1/").client(http)
        .addConverterFactory(GsonConverterFactory.create()).build()

    @Provides @Singleton
    fun provideApi(r: Retrofit): FluidApi = r.create(FluidApi::class.java)
}

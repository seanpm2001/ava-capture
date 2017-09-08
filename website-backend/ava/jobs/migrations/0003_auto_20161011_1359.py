# -*- coding: utf-8 -*-
# Generated by Django 1.10.1 on 2016-10-11 17:59
from __future__ import unicode_literals

from django.db import migrations, models
import django.db.models.deletion


class Migration(migrations.Migration):

    dependencies = [
        ('jobs', '0002_auto_20161011_1336'),
    ]

    operations = [
        migrations.AddField(
            model_name='farmjob',
            name='exception',
            field=models.CharField(max_length=800, null=True),
        ),
        migrations.AddField(
            model_name='farmjob',
            name='node',
            field=models.ForeignKey(null=True, on_delete=django.db.models.deletion.CASCADE, to='jobs.FarmNode'),
        ),
        migrations.AddField(
            model_name='farmjob',
            name='params',
            field=models.CharField(max_length=800, null=True),
        ),
    ]
